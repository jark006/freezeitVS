#pragma once

#include <sstream>
#include <fstream>
#include <string>
#include <string_view>
#include <chrono>
#include <thread>
#include <vector>
#include <memory>
#include <atomic>
#include <mutex>
#include <limits>
#include <set>
#include <unordered_set>
#include <map>

#include <cstdio>
#include <cerrno>
#include <cstring>
#include <ctime>
#include <cmath>
#include <cstdarg>
#include <cstdlib>
#include <csignal>
#include <cctype>

#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <sched.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <linux/input.h>
#include <linux/android/binder.h>

#include <sys/un.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/inotify.h>
#include <sys/sysinfo.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <sys/prctl.h>
#include <sys/mount.h>
#include <sys/system_properties.h>

using std::set;
using std::unordered_set;
using std::map;
using std::multimap;
using std::stringstream;
using std::lock_guard;
using std::unique_ptr;
using std::ifstream;
using std::vector;
using std::string;
using std::string_view;
using std::thread;
using std::mutex;

using std::make_unique;
using std::to_string;
using std::move;


// 配置编译选项 *****************
constexpr auto FORK_DOUBLE = 1;

#define DEBUG_LOG            1
#define DEBUG_DURATION       0
// *****************************

#if DEBUG_LOG
#define DLOG(...) freezeit.logFmt(__VA_ARGS__)
#else
#define DLOG(...) ((void)0)
#endif

#if DEBUG_DURATION
#define START_TIME_COUNT auto start_clock = clock()
#if CLOCKS_PER_SEC == 1000000
#define END_TIME_COUNT int duration_us=clock()-start_clock;freezeit.logFmt("%s(): %d.%03d ms", __FUNCTION__, duration_us/1000, duration_us%1000)
#elif CLOCKS_PER_SEC == 1000
#define END_TIME_COUNT int duration_ms=clock()-start_clock;freezeit.logFmt("%s(): %d ms", __FUNCTION__, duration_ms)
#else
#error CLOCKS_PER_SEC value is not support
#endif
#else
#define START_TIME_COUNT ((void)0)
#define END_TIME_COUNT   ((void)0)
#endif

#define SYNC_RECEIVED_WHILE_FROZEN (1)
#define ASYNC_RECEIVED_WHILE_FROZEN (2)
#define TXNS_PENDING_WHILE_FROZEN (4)

enum class WORK_MODE : uint32_t {
    GLOBAL_SIGSTOP = 0,
    V1 = 1,
    V1_ST = 2,
    V2UID = 3,
    V2FROZEN = 4,
};

enum class FREEZE_MODE : uint32_t {
    TERMINATE = 10,
    SIGNAL = 20,
    SIGNAL_BREAK = 21,
    FREEZER = 30,
    FREEZER_BREAK = 31,
    WHITELIST = 40,
    WHITEFORCE = 50,
};

enum class XPOSED_CMD : uint32_t {
    // 1359322925 是 "Freezeit" 的10进制CRC32值
    GET_FOREGROUND = 1359322925 + 1,
    GET_SCREEN = 1359322925 + 2,

    SET_CONFIG = 1359322925 + 20,
    SET_WAKEUP_LOCK = 1359322925 + 21,

    BREAK_NETWORK = 1359322925 + 41,
};

enum class REPLY : uint32_t {
    SUCCESS = 2, // 成功
    FAILURE = 0, // 失败
};

enum class WAKEUP_LOCK : uint32_t {
    IGNORE = 1,
    DEFAULT = 3,
};


enum class MANAGER_CMD : uint32_t {
    // 获取信息 无附加数据 No additional data required
    getPropInfo = 2,     // return string: "ID\nName\nVersion\nVersionCode\nAuthor\nclusterNum"
    getChangelog = 3,    // return string: "changelog"
    getLog = 4,          // return string: "log"
    getAppCfg = 5,       // return string: "package x\npackage x\n...
    getRealTimeInfo = 6, // return ImgBytes[h*w*4]+String: (rawBitmap + 内存 频率 使用率 电流)
    getSettings = 8,     // return bytes[256]: all settings parameter
    getUidTime = 9,      // return "uid last_user_time last_sys_time user_time sys_time\n..."

    // 设置 需附加数据
    setAppCfg = 21,      // send "package x\npackage x\npackage x\n..."
    setAppLabel = 22,    // send "uid label\nuid label\nuid label\n..."
    setSettingsVar = 23, // send bytes[2]: [0]index [1]value

    // 其他命令 无附加数据 No additional data required
    clearLog = 61,       // return string: "log" //清理并返回log
    getProcState = 62, // return string: "log" //打印冻结状态并返回log

};

struct binder_state {
    int fd;
    void* mapped;
    size_t mapSize;
};

struct KernelVersionStruct {
    int main = 0;
    int sub = 0;
    int patch = 0;
};

struct MemInfoStruct { // Unit: MiB
    int totalRam = 1;
    int availRam = 1;
    int totalSwap = 1;
    int freeSwap = 1;
};

struct cpuRealTimeStruct {
    int freq;  // Mhz
    int usage; // %
};

struct uidTimeStruct {
    int lastTotal = 0;
    int total = 0;
};

struct appInfoStruct {
    int uid = -1;
    FREEZE_MODE freezeMode = FREEZE_MODE::FREEZER; // [10]:杀死 [20]:SIGSTOP [30]:freezer [40]:配置 [50]:内置
    bool isTolerant = true;        // 宽容的 有前台服务也算前台
    int failFreezeCnt = 0;         // 冻结失败计数
    bool isSystemApp = true;       // 是否系统应用
    time_t startTimestamp = 0;     // 某次开始运行时刻
    time_t stopTimestamp = 0;      // 某次冻结运行时刻
    time_t totalRunningTime = 0;   // 运行时长
    string package;                // 包名
    string label;                  // 名称
    vector<int> pids;              // PID列表

    bool needBreakNetwork() const {
        return freezeMode == FREEZE_MODE::SIGNAL_BREAK || freezeMode == FREEZE_MODE::FREEZER_BREAK;
    }
    bool isSignalMode() const {
        return freezeMode == FREEZE_MODE::SIGNAL_BREAK || freezeMode == FREEZE_MODE::SIGNAL;
    }
    bool isFreezeMode() const {
        return freezeMode == FREEZE_MODE::FREEZER_BREAK || freezeMode == FREEZE_MODE::FREEZER;
    }
    bool isSignalOrFreezer() const {
        return freezeMode <= FREEZE_MODE::SIGNAL && freezeMode < FREEZE_MODE::WHITELIST;
    }
    bool isWhitelist() const {
        return freezeMode >= FREEZE_MODE::WHITELIST;
    }
    bool isBlacklist() const {
        return freezeMode < FREEZE_MODE::WHITELIST;
    }
    bool isTerminateMode() const {
        return freezeMode == FREEZE_MODE::TERMINATE;
    }
};

struct cfgStruct {
    FREEZE_MODE freezeMode = FREEZE_MODE::FREEZER;
    bool isTolerant = true;
};

template<size_t CAPACITY=32>
class stackString {
public:
    size_t length{ 0 };
    char data[CAPACITY];

    const char* c_str() { return data; }
    const char* operator* () { return data; }

    stackString() { data[0] = 0; }
    stackString(const string_view& s) {
        memcpy(data, s.data(), s.length());
        length = s.length();
        data[length] = 0;
    }
    stackString(const char* s, const size_t len) {
        memcpy(data, s, len);
        length = len;
        data[length] = 0;
    }

    stackString& append(const int n) {
        char tmp[16];
        return append(tmp, static_cast<size_t>(snprintf(tmp, sizeof(tmp), "%d", n)));
    }

    stackString& append(const char* s) {
        return append(s, strlen(s));
    }

    stackString& append(const char c) {
        if (length + 1 >= CAPACITY) // 预留最后一位 填 '\0'
            return *this;

        data[length++] = c;
        data[length] = 0;
        return *this;
    }

    stackString& append(const char* s, const size_t len) {
        if (length + len >= CAPACITY) // 预留最后一位 填 '\0'
            return *this;

        memcpy(data + length, s, len);
        length += len;
        data[length] = 0;
        return *this;
    }

    // 注意长度，可能溢出，不安全
    template<typename... Args>
    stackString& appendFmt(const char* fmt, Args&&... args) {
        if (length < CAPACITY)
            length += snprintf(data + length, CAPACITY - length, fmt, std::forward<Args>(args)...);
        return *this;
    }

    void clear() { length = 0;  data[0] = 0; }
};


namespace Utils {

    vector<string> splitString(const string& str, const string& delim) {
        if (str.empty()) return {};
        if (delim.empty()) return { str };

        vector<string> res;
        size_t nextDelimIdx, targetBeginIdx = 0;
        while ((nextDelimIdx = str.find(delim, targetBeginIdx)) != string::npos) {
            if (nextDelimIdx == targetBeginIdx) {
                targetBeginIdx += delim.length();
                continue;
            }
            res.emplace_back(str.substr(targetBeginIdx, nextDelimIdx - targetBeginIdx));
            targetBeginIdx = nextDelimIdx + delim.length();
        }
        if (targetBeginIdx < str.length())
            res.emplace_back(str.substr(targetBeginIdx, str.length() - targetBeginIdx));
        return res;
    }

    void strReplace(string& src, const string& oldBlock, const string& newBlock) {
        if (oldBlock.empty())return;

        size_t nextBeginIdx = 0, foundIdx;
        while ((foundIdx = src.find(oldBlock, nextBeginIdx)) != string::npos) {
            src.replace(foundIdx, oldBlock.length(), newBlock);

            // 替换后，在新区块【后面】开始搜索
            nextBeginIdx = foundIdx + newBlock.length(); 

            // 替换后，以新区块【起点】开始搜索。即：替换之后的新区块连同【后续】内容仍有可能满足条件而被继续替换
            // nextBeginIdx = foundIdx;

            // 替换后，在新区块【前后】搜索，新区块连同【前-后】内容仍有可能满足条件而被继续替换
            // nextBeginIdx = foundIdx > oldBlock.length() ? (foundIdx - oldBlock.length()) : 0;
        }
        return;
    }

    string bin2Hex(const void* bytes, const int len) {
        auto charList = "0123456789ABCDEF";
        if (len == 0) return "";
        string res(len * 3, ' ');
        for (int i = 0; i < len; i++) {
            const uint8_t value = reinterpret_cast<const uint8_t*>(bytes)[i];
            res[i * 3L] = charList[value >> 4];
            res[i * 3L + 1] = charList[value & 0x0f];
        }
        return res;
    }

    //"2022-01-01 00:00:00"
    time_t timeFormat2Timestamp(const char* strTimeFormat) {
        // strTimeFormat should be such as "2001-11-12 18:31:01"
        struct tm timeinfo;
        memset((void*)&timeinfo, 0, sizeof(struct tm));

        // strptime("1970:01:01 08:00:00", "%Y:%m:%d %H:%M:%S", timeinfo);
        strptime(strTimeFormat, "%Y-%m-%d %H:%M:%S", &timeinfo);

        return mktime(&timeinfo);
    }

    // https://blog.csdn.net/lanmanck/article/details/8423669
    vector<int> getTouchEventNum() {
        vector<int> res;

        for (int i = 0; i < 16; i++) {
            char path[64];
            snprintf(path, 64, "/dev/input/event%d", i);
            auto fd = open(path, O_RDONLY, 0);
            if (fd < 0)continue;

            uint32_t flagBit = 0;
            constexpr uint32_t cmd = EVIOCGBIT(0, sizeof(uint32_t));
            ioctl(fd, cmd, &flagBit);
            if (flagBit & (1 << EV_ABS)) res.emplace_back(i);
            close(fd);
        }
        if (res.size() == 0) {
            fprintf(stderr, "前台任务同步事件获取失败");
            exit(-1);
        }
        return res;
    }

    void myDecode(const void* _ptr, int len) {
        auto ptr = (uint8_t*)_ptr;
        while (len--) {
            *ptr ^= 0x91;
            ptr++;
        }
    }

    int readInt(const char* path) {
        auto fd = open(path, O_RDONLY);
        if (fd < 0) return 0;
        char buff[16] = { 0 };
        auto len = read(fd, buff, sizeof(buff));
        close(fd);

        if (len <= 0)return 0;
        buff[15] = 0;
        return atoi(buff);
    }

    size_t readString(const char* path, char* buff, const size_t maxLen) {
        auto fd = open(path, O_RDONLY);
        if (fd <= 0) {
            buff[0] = 0;
            return 0;
        }
        ssize_t len = read(fd, buff, maxLen);
        close(fd);
        if (len <= 0) {
            buff[0] = 0;
            return 0;
        }
        buff[len] = 0; // 终止符
        return static_cast<size_t>(len);
    }

    size_t popenRead(const char* cmd, char* buf, const size_t maxLen) {
        auto fp = popen(cmd, "r");
        if (!fp) return 0;
        auto readLen = fread(buf, 1, maxLen, fp);
        pclose(fp);
        return readLen;
    }

    // 最大读取 64 KiB
    string readString(const char* path) {
        char buff[64 * 1024];
        readString(path, buff, sizeof(buff));
        return string(buff);
    }

    bool writeInt(const char* path, const int value) {
        auto fd = open(path, O_WRONLY);
        if (fd <= 0) return false;

        char tmp[16];
        auto len = snprintf(tmp, sizeof(tmp), "%d", value);
        write(fd, tmp, len);
        close(fd);
        return true;
    }

    bool writeString(const char* path, const char* buff, size_t len = 0) {
        if (len == 0)len = strlen(buff);
        if (len == 0)return true;

        auto fd = open(path, O_WRONLY | O_TRUNC | O_CREAT, 0666);
        if (fd <= 0) return false;

        write(fd, buff, len);
        close(fd);
        return true;
    }

    char lastChar(char* ptr) {
        if (!ptr)return 0;
        while (*ptr) ptr++;
        return *(ptr - 1);
    }

    bool startWith(const char* prefix, const char* target) {
        int idx = 0;
        while (prefix[idx]) {
            if (prefix[idx] != target[idx])
                return false;
            idx++;
        }
        return true;
    }

    bool endWith(const string& suffix, const string& target) {
        if (suffix.empty() || suffix.length() > target.length()) return false;
        for (int i = suffix.length() - 1, j = target.length() - 1; i >= 0; i--, j--) {
            if (suffix[i] != target[j]) return false;
        }
        return true;
    }

    string parentDir(string path) {
        if (path.empty())return "";
        if (path.back() == '/') path.pop_back();
        auto idx = path.find_last_of('/');
        return idx == string::npos ? path : path.substr(0, idx);
    }

    int localSocketRequest(
        const XPOSED_CMD requestCode,
        const void* payloadBuff,
        const int payloadLen,
        int* recvBuff,
        const size_t maxRecvLen) {

        // Socket 位于Linux抽象命名空间， 而不是文件路径
        // https://blog.csdn.net/howellzhu/article/details/111597734
        // https://blog.csdn.net/shanzhizi/article/details/16882087 一种是路径方式 一种是抽象命名空间
        constexpr int addrLen =
            offsetof(sockaddr_un, sun_path) + 21; // addrLen大小是 "\0FreezeitXposedServer" 的字符长度
        constexpr sockaddr_un srv_addr{ AF_UNIX, "\0FreezeitXposedServer" }; // 首位为空[0]=0，位于Linux抽象命名空间

        auto fd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (fd < 0)
            return -10;

        if (connect(fd, (sockaddr*)&srv_addr, addrLen) < 0) {
            close(fd);
            return 0;
        }

        int header[2] = { static_cast<int>(requestCode), payloadLen };
        send(fd, header, sizeof(header), 0);

        int sendCnt = 0;
        while (sendCnt < payloadLen) {
            int len = send(fd, static_cast<const char*>(payloadBuff) + sendCnt,
                static_cast<size_t>(payloadLen - sendCnt), 0);
            if (len < 0) {
                close(fd);
                return -20;
            }
            sendCnt += len;
        }

        int recvLen = recv(fd, recvBuff, maxRecvLen, MSG_WAITALL);
        close(fd);
        return recvLen;
    }


    void printException(
        const char* versionStr, 
        const int exceptionCnt, 
        const char* exceptionBuf,
        const size_t bufSize) {
        auto fp = fopen("/sdcard/Android/freezeit_crash_log.txt", "ab");
        if (!fp) return;

        auto timeStamp = time(nullptr);
        auto tm = localtime(&timeStamp);

        fprintf(fp, "[%04d-%02d-%02d %02d:%02d:%02d] ",
            tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);

        if (versionStr)fprintf(fp, "[%s] ", versionStr);
        if (exceptionCnt) fprintf(fp, "第%d次异常 ", exceptionCnt);

        fwrite(exceptionBuf, 1, bufSize, fp);
        if (exceptionBuf[bufSize - 1] != '\n')
            fwrite("\n", 1, 1, fp);
        fclose(fp);
    }


    void Init() {
        auto now = std::chrono::system_clock::now();
        auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
        srand(ns);
        usleep(1000 * (rand() & 0x7ff)); //随机休眠 1ms ~ 2s

        // 检查是否还有其他freezeit进程，防止进程多开
        char buf[256] = { 0 };
        if (popenRead("pidof freezeit", buf, sizeof(buf)) == 0) {
            printException(nullptr, 0, "进程检测失败", 18);
            exit(-1);
        }

        auto ptr = strchr(buf, ' ');
        if (ptr) { // "pidNum1 pidNum2 ..."  如果存在多个pid就退出
            *ptr = 0;
            char tips[256];
            auto len = snprintf(tips, sizeof(tips),
                "冻它已经在运行(pid: %s), 当前进程(pid:%d)即将退出，"
                "请勿手动启动冻它, 也不要在多个框架同时安装冻它模块", buf, getpid());
            printf("\n!!! \n!!! %s\n!!!\n\n", tips);
            printException(nullptr, 0, tips, len);
            exit(-2);
        }


        if (FORK_DOUBLE == 0)
            return;

        pid_t pid = fork();

        if (pid < 0) { //创建失败
            printException(nullptr, 0, "脱离终端Fork失败", 22);
            exit(-1);
        }
        else if (pid > 0) { //父进程返回的是 子进程的pid
            exit(0);//父进程直接退出，然后子进程将由init托管
        }

        setsid();// 子进程 建立新会话
        umask(0);
        chdir("/");

        // signal(SIGCHLD, SIG_IGN);//屏蔽SIGCHLD信号 通知内核对子进程的结束不关心，由内核回收
        int fd_response[2];
        pipe(fd_response);

        pid = fork(); //成为守护进程后再次Fork, 父进程监控， 子进程工作
        if (pid < 0) {
            printException(nullptr, 0, "创建工作进程Fork失败", 28);
            exit(-1);
        }
        else if (pid > 0) { //父进程 监控子进程输出的异常信息，并写到异常日志
            close(fd_response[1]); // 1 关闭写端

            char versionStr[16] ="Unknown";
            char exceptionBuf[4096] = {};
            int exceptionCnt = 0;
            int zeroCnt = 0;

            while (true) {

                auto readLen = read(fd_response[0], exceptionBuf, sizeof(exceptionBuf));
                if (readLen <= 0) {
                    readLen = snprintf(exceptionBuf, 64, "[第%d次无效日志]", ++zeroCnt);
                }
                else if (!strncmp(exceptionBuf, "version ", 8)) {
                    memcpy(versionStr, exceptionBuf + 8, sizeof(versionStr));
                    continue;
                }

                printException(versionStr, ++exceptionCnt, exceptionBuf, readLen);

                if (zeroCnt >= 3 || exceptionCnt >= 1000) {
                    if (zeroCnt >= 3)
                        printException(versionStr, 0, "工作进程已异常退出", 27);
                    else
                        printException(versionStr, 0, "工作进程已达最大异常次数, 即将强制关闭", 56);

                    if (kill(pid, SIGKILL) < 0) {
                        char tips[128];
                        auto len = snprintf(tips, sizeof(tips), "杀死 [工作进程 pid:%d] 失败", pid);
                        printException(versionStr, 0, tips, len);
                    }

                    int status = 0;
                    if (waitpid(pid, &status, __WALL) != pid) {
                        char tips[128];
                        auto len = snprintf(tips, sizeof(tips), "waitpid 异常: [%d] HEX[%s]", status,
                            bin2Hex(&status, 4).c_str());
                        printException(versionStr, 0, tips, len);
                    }

                    exit(-1);
                }
            }
        }

        //工作进程
        close(fd_response[0]); // 0 关闭读端

        // 标准输出和错误均指向父进程管道
        // dup2(fd_response[1], STDOUT_FILENO); // 把 system() shell 标准输出到异常日志
        dup2(fd_response[1], STDERR_FILENO);

        auto nullFd = open("/dev/null", O_RDWR);
        if (nullFd > 0) {
            dup2(nullFd, STDIN_FILENO);
            dup2(nullFd, STDOUT_FILENO);
        }
        else {
            close(STDIN_FILENO);
            close(STDOUT_FILENO);
        }
    }
}


namespace MAGISK {
    int get_version_code() {
        char buff[32] = { 0 };
        Utils::popenRead("/system/bin/magisk -V", buff, sizeof(buff));
        return isdigit(buff[0]) ? atoi(buff) : -1;
    }
}

// https://github.com/tiann/KernelSU/blob/main/manager/app/src/main/cpp/ksu.cc
namespace KSU {
    const int CMD_GRANT_ROOT = 0;
    const int CMD_BECOME_MANAGER = 1;
    const int CMD_GET_VERSION = 2;
    const int CMD_ALLOW_SU = 3;
    const int CMD_DENY_SU = 4;
    const int CMD_GET_ALLOW_LIST = 5;
    const int CMD_GET_DENY_LIST = 6;
    const int CMD_CHECK_SAFEMODE = 9;

    bool ksuctl(int cmd, void* arg1, void* arg2) {
        const uint32_t KERNEL_SU_OPTION{ 0xDEADBEEF };
        uint32_t result = 0;
        prctl(KERNEL_SU_OPTION, cmd, arg1, arg2, &result);
        return result == KERNEL_SU_OPTION;
    }

    int get_version_code() {
        int version = -1;
        ksuctl(CMD_GET_VERSION, &version, nullptr);
        return version;
    }

    bool allow_su(uint64_t uid, bool allow) {
        return ksuctl(allow ? CMD_ALLOW_SU : CMD_DENY_SU, (void*)uid, nullptr);
    }

    bool get_allow_list(int* uids, int* size) {
        return ksuctl(CMD_GET_ALLOW_LIST, uids, size);
    }

    bool get_deny_list(int* uids, int* size) {
        return ksuctl(CMD_GET_DENY_LIST, uids, size);
    }

    bool is_safe_mode() {
        return ksuctl(CMD_CHECK_SAFEMODE, nullptr, nullptr);
    }
};
