#pragma once

#include "utils.hpp"
#include "freezeit.hpp"
#include "managedApp.hpp"
#include "systemTools.hpp"
#include "freezer.hpp"
#include "doze.hpp"

class Server {
private:
    Freezeit& freezeit;
    Settings& settings;
    ManagedApp& managedApp;
    SystemTools& systemTools;
    Freezer& freezer;
    Doze& doze;

    thread serverThread;

    static const int RECV_BUF_SIZE = 2 * 1024 * 1024;  // 2 MiB TCP通信接收缓存大小
    static const int REPLY_BUF_SIZE = 8 * 1024 * 1024; // 8 MiB TCP通信回应缓存大小
    unique_ptr<char[]> recvBuf, replyBuf;

public:
    Server& operator=(Server&&) = delete;

    Server(Freezeit& freezeit, Settings& settings, ManagedApp& managedApp, SystemTools& systemTools,
        Doze& doze, Freezer& freezer) :
        freezeit(freezeit), settings(settings), managedApp(managedApp),
        systemTools(systemTools), freezer(freezer), doze(doze) {
        serverThread = thread(&Server::serverThreadFunc, this);
    }

    void serverThreadFunc() {
        /*  LOCAL_SOCKET  *******************************************************************/
        // Socket 位于Linux抽象命名空间， 而不是文件路径
        // https://blog.csdn.net/howellzhu/article/details/111597734
        // https://blog.csdn.net/shanzhizi/article/details/16882087 一种是路径方式 一种是抽象命名空间
        //const int addrLen = offsetof(sockaddr_un, sun_path) + 15; // addrLen大小是 首个占位符 '\0' 加 "FreezeitServer" 的字符长度
        //const sockaddr_un serv_addr{ AF_UNIX, "\0FreezeitServer" }; // 首位为空[0]=0，位于Linux抽象命名空间
        //sockaddr_un clnt_addr{};
        //socklen_t clnt_addr_size = sizeof(sockaddr_un);
        // 
        // 终端执行 setenforce 0 ，即设置 SELinux 为宽容模式, 普通安卓应用才可以使用 LocalSocket
        //system("setenforce 0");
        /*  LOCAL_SOCKET  *******************************************************************/

        constexpr socklen_t addrLen = sizeof(sockaddr);
        const sockaddr_in serv_addr{ AF_INET, htons(60613), {inet_addr("127.0.0.1")}, {} };


        recvBuf = make_unique<char[]>(RECV_BUF_SIZE);
        replyBuf = make_unique<char[]>(REPLY_BUF_SIZE);

        while (true) {
            static int failTcpCnt = 0;
            if (failTcpCnt) {
                fprintf(stderr, "Socket 失败%d次, [%d]:[%s]", failTcpCnt, errno, strerror(errno));
                if (failTcpCnt > 100) {
                    fprintf(stderr, "Socket 彻底失败, 已退出。[%d]:[%s]", errno, strerror(errno));
                    exit(-1);
                }
                sleep(5);
            }
            failTcpCnt++;

            int serv_sock;

            /*  LOCAL_SOCKET  *******************************************************************/
            //if ((serv_sock = socket(AF_UNIX, SOCK_STREAM, 0)) <= 0) {
            //	fprintf(stderr, "socket() Fail serv_sock[%d], [%d]:[%s]", serv_sock, errno, strerror(errno));
            //	continue;
            //}
            /*  LOCAL_SOCKET  *******************************************************************/


            /*  NORMAL_SOCKET  ******************************************************************/
            if ((serv_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) <= 0) {
                fprintf(stderr, "socket() Fail serv_sock[%d], [%d]:[%s]", serv_sock, errno,
                    strerror(errno));
                continue;
            }

            int opt = 1;  //地址和端口 释放后可立即重用 否则几分钟后才可使用
            if (setsockopt(serv_sock, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
                fprintf(stderr, "setsockopt() Fail serv_sock[%d], [%d]:[%s]", serv_sock, errno,
                    strerror(errno));
                continue;
            }
            /*  NORMAL_SOCKET  ******************************************************************/



            if (bind(serv_sock, (sockaddr*)&serv_addr, addrLen) < 0) {
                fprintf(stderr, "bind() Fail, [%d]:[%s]", errno, strerror(errno));
                continue;
            }

            if (listen(serv_sock, 4) < 0) {
                fprintf(stderr, "listen() Fail, [%d]:[%s]", errno, strerror(errno));
                continue;
            }

            while (true) {
                sockaddr_in clnt_addr{};
                socklen_t clnt_addr_size = sizeof(sockaddr_in);
                int clnt_sock = accept(serv_sock, (sockaddr*)&clnt_addr, &clnt_addr_size);
                if (clnt_sock < 0) {
                    static int failCnt = 1;

                    fprintf(stderr, "accept() 第%d次错误 servFd[%d] clntFd[%d] size[%d]; [%d]:[%s]",
                        failCnt, serv_sock, clnt_sock, clnt_addr_size, errno, strerror(errno));

                    if (++failCnt > 10) break;

                    sleep(2);
                    continue;
                }

                //设置接收超时
                timeval timeout = { 5, 0 }; // 5秒 超时
                if (setsockopt(clnt_sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout,
                    sizeof(timeval))) {
                    fprintf(stderr, "setsockopt 超时设置出错 servFd[%d] clntFd[%d] [%d]:[%s]", serv_sock,
                        clnt_sock, errno, strerror(errno));
                    close(clnt_sock);
                    continue;
                }

                uint8_t dataHeader[6];
                auto headerLen = recv(clnt_sock, dataHeader, sizeof(dataHeader), MSG_WAITALL);
                if (headerLen != sizeof(dataHeader)) {
                    close(clnt_sock);
                    fprintf(stderr, "clnt_sock recv dataHeader len[%ld]", headerLen);
                    continue;
                }

                uint32_t payloadLen = *((uint32_t*)dataHeader);
                uint32_t appCommand = dataHeader[4];
                uint32_t XOR_value = dataHeader[5];

                // "\0AUTH\n" Bilibili客户端乱发的，前4字节： 大端 4281684, 小端 1414873344
                if (payloadLen == 1414873344 || payloadLen == 4281684) {
                    close(clnt_sock);
                    continue;
                }
                else if (payloadLen >= RECV_BUF_SIZE) {
                    freezeit.logFmt("数据格式异常 payloadLen[%u] dataHeaderHEX[%s]", payloadLen,
                        Utils::bin2Hex(dataHeader, 6).c_str());
                    close(clnt_sock);
                    continue;
                }

                if (payloadLen) {
                    uint32_t lenTmp = recv(clnt_sock, recvBuf.get(), payloadLen, MSG_WAITALL);
                    if (lenTmp != payloadLen) {
                        fprintf(stderr, "附带数据接收错误, appCommand[%u], 要求[%u], 实际接收[%u]", 
                            appCommand,	payloadLen, lenTmp);
                        close(clnt_sock);
                        continue;
                    }

                    uint8_t XOR_cal = 0;
                    for (uint32_t i = 0; i < payloadLen; i++)
                        XOR_cal ^= (uint8_t)recvBuf[i];

                    if (XOR_value != XOR_cal) {
                        fprintf(stderr, "数据校验错误, 提供值[0x%2x], 接收数据计算值[0x%2x]", XOR_value, XOR_cal);
                        close(clnt_sock);
                        continue;
                    }
                }

                recvBuf[payloadLen] = 0;
                handleCmd(static_cast<MANAGER_CMD>(appCommand), payloadLen, clnt_sock);
            }
            close(serv_sock);
        }
    }

    void handleCmd(const MANAGER_CMD appCommand, const int recvLen, const int clnt_sock) {
        const char* replyPtr = nullptr;
        uint32_t replyLen = 0;
        switch (appCommand) {
        case MANAGER_CMD::getPropInfo: {
            replyPtr = replyBuf.get();
            replyLen = snprintf(replyBuf.get(), REPLY_BUF_SIZE,
                "%s\n%s\n%s\n%s\n%s\n%d\n%s\n%s\n%s\n%s\n%u",
                freezeit.prop["id"].c_str(), freezeit.prop["name"].c_str(),
                freezeit.prop["version"].c_str(), freezeit.prop["versionCode"].c_str(),
                freezeit.prop["author"].c_str(),
                systemTools.cpuCluster, freezeit.moduleEnv.c_str(), freezer.getCurWorkModeStr(),
                systemTools.androidVerStr.c_str(), systemTools.kernelVerStr.c_str(), systemTools.extMemorySize);
        } break;

        case MANAGER_CMD::getChangelog: {
            replyPtr = freezeit.getChangelogPtr();
            replyLen = freezeit.getChangelogLen();
        } break;

        case MANAGER_CMD::getLog: {
            replyPtr = freezeit.getLogPtr();
            replyLen = freezeit.getLoglen();
        } break;

        case MANAGER_CMD::getAppCfg: {
            uint32_t intCnt = 0;
            const auto ptr = reinterpret_cast<int*>(replyBuf.get());
            for (const auto& appInfo : managedApp.appInfoMap) {
                if (appInfo.uid < 0)continue;

                ptr[intCnt++] = appInfo.uid;
                ptr[intCnt++] = static_cast<int>(appInfo.freezeMode);
                ptr[intCnt++] = appInfo.isPermissive ? 1 : 0;
            }

            replyPtr = replyBuf.get();
            replyLen = intCnt * sizeof(int);
        } break;

        case MANAGER_CMD::getRealTimeInfo: {
            if (recvLen != 12) {
                replyPtr = replyBuf.get();
                replyLen = snprintf(replyBuf.get(), 128, "实时信息需要12字节, 实际收到[%u]",
                    recvLen);
                break;
            }

            uint32_t height = ((uint32_t*)recvBuf.get())[0];
            uint32_t width = ((uint32_t*)recvBuf.get())[1];

            if (height < 20 || width < 20) {
                replyPtr = replyBuf.get();
                replyLen = snprintf(replyBuf.get(), 128, "宽高不符合, height[%u] width[%u]",
                    height, width);
                break;
            }

            uint32_t availableMiB = ((uint32_t*)recvBuf.get())[2]; // Unit: MiB

            systemTools.getCPU_realtime(availableMiB);
            replyLen = systemTools.drawChart((uint32_t*)replyBuf.get(), height, width);
            replyLen += systemTools.formatRealTime(reinterpret_cast<int*>(replyBuf.get() + replyLen));
            replyPtr = replyBuf.get();
        } break;

        case MANAGER_CMD::getUidTime: {
            int* ptr = reinterpret_cast<int*>(replyBuf.get());
            struct st {
                int uid;
                int total;
                int lastTotal;
            };
            vector<st> uidTimeSort;
            uidTimeSort.reserve(256);

            for (const auto& [uid, timeList] : doze.updateUidTime())
                uidTimeSort.emplace_back(st{ uid, timeList.total, timeList.lastTotal });
            std::sort(uidTimeSort.begin(), uidTimeSort.end(),
                [](const st& a, const st& b) { return a.total > b.total; });

            int intCnt = 0;
            for (const auto& [uid, total, lastTotal] : uidTimeSort) {
                ptr[intCnt++] = uid;
                ptr[intCnt++] = total - lastTotal;
                ptr[intCnt++] = total;
            }

            replyPtr = replyBuf.get();
            replyLen = intCnt * sizeof(int);
        } break;

        case MANAGER_CMD::getXpLog: {
            const int len = Utils::localSocketRequest(XPOSED_CMD::GET_XP_LOG, nullptr, 0, (int*)replyBuf.get(), REPLY_BUF_SIZE);
            if (len == 0) {
                freezeit.log("getXpLog 工作异常, 请确认LSPosed中冻它勾选系统框架, 然后重启");
                replyPtr = "Freezeit's Xposed log is empty. ";
                replyLen = 32;
            }
            else {
                replyPtr = replyBuf.get();
                replyLen = len;
            }
        } break;

        case MANAGER_CMD::getSettings: {
            replyPtr = settings.get();
            replyLen = settings.size();
        } break;

        case MANAGER_CMD::setAppCfg: {
            if (recvLen == 0 || (recvLen % 12)) {
                replyPtr = replyBuf.get();
                replyLen = snprintf(replyBuf.get(), 128, "需要12字节的倍数, 实际收到 %d 字节",
                    recvLen);
                freezeit.log(string_view(replyBuf.get(), replyLen));
                break;
            }

            managedApp.updateAppList();

            const int intSize = recvLen >> 2; // recvLen/4
            const int* ptr = reinterpret_cast<int*>(recvBuf.get());
            map<int, cfgStruct> newCfg;

            for (int i = 0; i < intSize;) {
                const int uid = ptr[i++];
                const FREEZE_MODE freezeMode = static_cast<FREEZE_MODE>(ptr[i++]);
                const bool isPermissive = ptr[i++] != 0;
                if (managedApp.contains(uid)) {
                    if (managedApp.FREEZE_MODE_SET.contains(freezeMode))
                        newCfg[uid] = { freezeMode, isPermissive };
                    else
                        freezeit.logFmt("错误配置: UID:%d freezeMode:%d isPermissive:%d", uid,
                            freezeMode, isPermissive);
                }
                else
                    freezeit.logFmt("特殊应用: UID:%d, 已强制自由后台", uid);
            }

            string tips;
            set<int> changeUidSet; // 发生配置变化的应用
            for (const auto& appInfo : managedApp.appInfoMap) {
                if (appInfo.uid < ManagedApp::UID_START || appInfo.freezeMode == FREEZE_MODE::WHITEFORCE ||
                    !newCfg.contains(appInfo.uid) || appInfo.freezeMode == newCfg[appInfo.uid].freezeMode)
                    continue;

                changeUidSet.insert(appInfo.uid);
                tips += freezer.getModeText(appInfo.freezeMode) + "->" +
                    freezer.getModeText(newCfg[appInfo.uid].freezeMode) + " [" +
                    appInfo.label + "]\n";
            }
            if (tips.length())
                freezeit.logFmt("配置变化：\n\n%s", tips.c_str());

            tips.clear();
            for (const auto& [uid, cfg] : newCfg) {
                if (!managedApp.contains(uid)) {
                    tips += to_string(uid) + ", ";
                }
            }
            if (tips.length())
                freezeit.logFmt("以下UID的应用不受冻它管理：[%s] 可在冻它配置页搜索UID查看是哪些应用", tips.c_str());

            // auto runningUids = freezer.getRunningUids(changeUidSet);
            auto runningPids = freezer.getRunningPids(changeUidSet);
            tips.clear();
            for (auto& [uid, pids] : runningPids) {
                auto& appInfo = managedApp[uid];
                tips += "\n" + appInfo.label;
                for (const int pid : pids)
                    tips += " " + to_string(pid);
                appInfo.pids = std::move(pids);
                freezer.handleSignal(appInfo, SIGKILL); // 已包含 V1 前置解冻
            }
            if (tips.length())
                freezeit.logFmt("杀死策略变更的应用: \n%s\n", tips.c_str());

            managedApp.loadConfig2CfgTemp(newCfg);
            managedApp.updateIME2CfgTemp();
            managedApp.applyCfgTemp();
            managedApp.saveConfig();
            managedApp.update2xposedByLocalSocket();

            replyPtr = "success";
            replyLen = 7;
        } break;

        case MANAGER_CMD::setAppLabel: {
            managedApp.updateAppList(); // 先更新应用列表

            map<int, string> labelList;
            for (const string& str : Utils::splitString(string(recvBuf.get(), recvLen),
                "\n")) {
                const int uid = atoi(str.c_str());
                if (!managedApp.contains(uid) || str.length() <= 6)
                    freezeit.logFmt("解析名称错误 [%s]", str.c_str());
                else labelList[uid] = str.substr(6);
            }

            string labelStr;
            labelStr.reserve(1024L * 4);
            for (const auto& [uid, label] : labelList) {
                labelStr += " [";
                labelStr += label;
                labelStr += ']';
            }
            freezeit.logFmt("更新 %lu 款应用名称:\n\n%s\n", labelList.size(), labelStr.c_str());

            managedApp.loadLabel(labelList);
            managedApp.update2xposedByLocalSocket();
            managedApp.saveLabel();

            replyPtr = "success";
            replyLen = 7;
        } break;

        case MANAGER_CMD::clearLog: {
            freezeit.clearLog();
            replyPtr = freezeit.getLogPtr();
            replyLen = freezeit.getLoglen();
        } break;

        case MANAGER_CMD::getProcState: {
            freezer.printProcState();
            replyPtr = freezeit.getLogPtr();
            replyLen = freezeit.getLoglen();
        } break;

        case MANAGER_CMD::setSettingsVar: {
            replyPtr = replyBuf.get();

            if (recvLen != 2) {
                replyLen = snprintf(replyBuf.get(), REPLY_BUF_SIZE,
                    "数据长度不正确, 正常:2, 收到:%d", recvLen);
                break;
            }

            int len = settings.checkAndSet(recvBuf[0], recvBuf[1], replyBuf.get());

            if (len <= 0) {
                memcpy(replyBuf.get(), "未知设置错误", 18);
                replyLen = 18;
            }
            else {
                replyLen = len;
            }
        } break;

        default: {
            replyPtr = "非法命令";
            replyLen = 12;
        } break;
        }

        if (replyLen) {
            uint32_t header[2] = { replyLen , 0 };
            send(clnt_sock, header, 6, MSG_DONTROUTE);
            send(clnt_sock, replyPtr, replyLen, MSG_DONTROUTE);
        }
        close(clnt_sock);
    }
};
