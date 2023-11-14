#pragma once

#include "utils.hpp"
#include "vpopen.hpp"

class Freezeit {
private:
    const char* LOG_PATH = "/sdcard/Android/freezeit.log";

    constexpr static int LINE_SIZE = 1024 * 32;   //  32 KiB
    constexpr static int BUFF_SIZE = 1024 * 128;  // 128 KiB

    mutex logPrintMutex;
    bool toFileFlag = false;
    size_t position = 0;
    char lineCache[LINE_SIZE] = "[00:00:00]  ";
    char logCache[BUFF_SIZE];

    string propPath;
    string changelog{ "无" };

    uint8_t* deBugFlagPtr = nullptr;

    // "Jul 28 2022" --> "2022-07-28"
    const char compilerDate[12] = {
            __DATE__[7],
            __DATE__[8],
            __DATE__[9],
            __DATE__[10],// YYYY year
            '-',

            // First month letter, Oct Nov Dec = '1' otherwise '0'
            (__DATE__[0] == 'O' || __DATE__[0] == 'N' || __DATE__[0] == 'D') ? '1' : '0',

            // Second month letter Jan, Jun or Jul
            (__DATE__[0] == 'J') ? ((__DATE__[1] == 'a') ? '1'
            : ((__DATE__[2] == 'n') ? '6' : '7'))
            : (__DATE__[0] == 'F') ? '2'// Feb
            : (__DATE__[0] == 'M') ? (__DATE__[2] == 'r') ? '3' : '5'// Mar or May
            : (__DATE__[0] == 'A') ? (__DATE__[1] == 'p') ? '4' : '8'// Apr or Aug
            : (__DATE__[0] == 'S') ? '9'// Sep
            : (__DATE__[0] == 'O') ? '0'// Oct
            : (__DATE__[0] == 'N') ? '1'// Nov
            : (__DATE__[0] == 'D') ? '2'// Dec
            : 'X',

            '-',
            __DATE__[4] == ' ' ? '0' : __DATE__[4],// First day letter, replace space with digit
            __DATE__[5],// Second day letter
        '\0',
    };

    void toMem(const char* logStr, const int len) {
        if ((position + len) >= BUFF_SIZE)
            position = 0;

        memcpy(logCache + position, logStr, len);
        position += len;
    }

    void toFile(const char* logStr, const int len) {
        auto fp = fopen(LOG_PATH, "ab");
        if (!fp) {
            fprintf(stderr, "日志输出(追加模式)失败 [%d][%s]", errno, strerror(errno));
            return;
        }

        auto fileSize = ftell(fp);
        if ((fileSize + len) >= BUFF_SIZE) {
            fclose(fp);
            usleep(1000);
            fp = fopen(LOG_PATH, "wb");
            if (!fp) {
                fprintf(stderr, "日志输出(超额清理模式)失败 [%d][%s]", errno, strerror(errno));
                return;
            }
        }

        fwrite(logStr, 1, len, fp);
        fclose(fp);
    }


public:

    bool isSamsung{ false };
    bool isOppoVivo{ false };


    string modulePath;
    string moduleEnv{ "Unknown" };

    map<string, string> prop{
            {"id",          "Unknown"},
            {"name",        "Unknown"},
            {"version",     "Unknown"},
            {"versionCode", "0"},
            {"author",      "Unknown"},
            {"description", "Unknown"},
    };

    Freezeit& operator=(Freezeit&&) = delete;

    Freezeit(int argc, const string& fullPath) {

        modulePath = Utils::parentDir(fullPath);

        int versionCode = -1;
        if (!access("/system/bin/magisk", F_OK)) {
            moduleEnv = "Magisk";
            versionCode = MAGISK::get_version_code();
            if (versionCode <= 0) {
                sleep(2);
                versionCode = MAGISK::get_version_code();
            }
        }
        else if (!access("/data/adb/ksud", F_OK)) {
            moduleEnv = "KernelSU";
            versionCode = KSU::get_version_code();
            if (versionCode <= 0) {
                sleep(2);
                versionCode = KSU::get_version_code();
            }
        }
        if (versionCode > 0)
            moduleEnv += " (" + to_string(versionCode) + ")";

        auto fp = fopen((modulePath + "/boot.log").c_str(), "rb");
        if (fp) {
            auto len = fread(logCache, 1, BUFF_SIZE, fp);
            if (len > 0)
                position = len;
            fclose(fp);
        }

        toFileFlag = argc > 1;
        if (toFileFlag) {
            if (position)toFile(logCache, position);
            const char tips[] = "日志已通过文件输出: /sdcard/Android/freezeit.log";
            toMem(tips, sizeof(tips) - 1);
        }

        propPath = modulePath + "/module.prop";
        fp = fopen(propPath.c_str(), "r");
        if (!fp) {
            fprintf(stderr, "找不到模块属性文件 [%s]", propPath.c_str());
            exit(-1);
        }

        char tmp[1024 * 4];
        while (!feof(fp)) {
            fgets(tmp, sizeof(tmp), fp);
            if (!isalpha(tmp[0])) continue;
            tmp[sizeof(tmp) - 1] = 0;
            auto ptr = strchr(tmp, '=');
            if (!ptr)continue;

            *ptr = 0;
            for (size_t i = (ptr - tmp) + 1; i < sizeof(tmp); i++) {
                if (tmp[i] == '\n' || tmp[i] == '\r') {
                    tmp[i] = 0;
                    break;
                }
            }
            prop[string(tmp)] = string(ptr + 1);
        }
        fclose(fp);


        changelog = Utils::readString((modulePath + "/changelog.txt").c_str());


        logFmt("模块版本 %s(%s)", prop["version"].c_str(), prop["versionCode"].c_str());
        logFmt("编译时间 %s %s UTC+8", compilerDate, __TIME__);

        fprintf(stderr, "version %s", prop["version"].c_str()); // 发送当前版本信息给监控进程

        char res[256];
        if (__system_property_get("gsm.operator.alpha", res) > 0 && res[0] != ',')
            logFmt("运营信息 %s", res);
        if (__system_property_get("gsm.network.type", res) > 0) logFmt("网络类型 %s", res);
        if (__system_property_get("ro.product.brand", res) > 0) {
            logFmt("设备厂商 %s", res);

            //for (int i = 0; i < 8; i++)res[i] |= 32;
            *((uint64_t*)res) |= 0x20202020'20202020ULL; // 转为小写
            if (!strncmp(res, "samsung", 7))
                isSamsung = true;
            else if (!strncmp(res, "oppo", 4) || !strncmp(res, "vivo", 4) ||
                !strncmp(res, "realme", 6) || !strncmp(res, "iqoo", 4))
                isOppoVivo = true;
        }
        if (__system_property_get("ro.product.marketname", res) > 0) logFmt("设备型号 %s", res);
        if (__system_property_get("persist.sys.device_name", res) > 0) logFmt("设备名称 %s", res);
        if (__system_property_get("ro.system.build.version.incremental", res) > 0)
            logFmt("系统版本 %s", res);
        if (__system_property_get("ro.soc.manufacturer", res) > 0 &&
            __system_property_get("ro.soc.model", res + 100) > 0)
            logFmt("硬件平台 %s %s", res, res + 100);
    }

    void setDebugPtr(uint8_t* ptr) {
        deBugFlagPtr = ptr;
    }
    bool isDebugOn() {
        if (deBugFlagPtr == nullptr)
            return false;

        return *deBugFlagPtr;
    }

    char* getChangelogPtr() { return (char*)changelog.c_str(); }

    size_t getChangelogLen() { return changelog.length(); }

    bool saveProp() {
        auto fp = fopen(propPath.c_str(), "wb");
        if (!fp)
            return false;

        char tmp[1024];
        size_t len = snprintf(tmp, sizeof(tmp),
            "id=%s\nname=%s\nversion=%s\nversionCode=%s\nauthor=%s\ndescription=%s\nupdateJson=%s\n",
            prop["id"].c_str(), prop["name"].c_str(), prop["version"].c_str(),
            prop["versionCode"].c_str(),
            prop["author"].c_str(), prop["description"].c_str(),
            prop["updateJson"].c_str());

        size_t writeLen = fwrite(tmp, 1, len, fp);
        fclose(fp);

        return (writeLen == len);
    }


    int formatTimePrefix() {
        time_t timeStamp = time(nullptr) + 8 * 3600L;
        int hour = (timeStamp / 3600) % 24;
        int min = (timeStamp % 3600) / 60;
        int sec = timeStamp % 60;

        //lineCache[LINE_SIZE] = "[00:00:00] ";
        lineCache[1] = (hour / 10) + '0';
        lineCache[2] = (hour % 10) + '0';
        lineCache[4] = (min / 10) + '0';
        lineCache[5] = (min % 10) + '0';
        lineCache[7] = (sec / 10) + '0';
        lineCache[8] = (sec % 10) + '0';

        return 11;
    }

    int formatTimeDebug() {
        time_t timeStamp = time(nullptr) + 8 * 3600L;
        int hour = (timeStamp / 3600) % 24;
        int min = (timeStamp % 3600) / 60;
        int sec = timeStamp % 60;

        //lineCache[LINE_SIZE] = "[00:00:00] DEBUG ";
        lineCache[1] = (hour / 10) + '0';
        lineCache[2] = (hour % 10) + '0';
        lineCache[4] = (min / 10) + '0';
        lineCache[5] = (min % 10) + '0';
        lineCache[7] = (sec / 10) + '0';
        lineCache[8] = (sec % 10) + '0';

        memcpy(lineCache + 11, "DEBUG ", 6);

        return 17;
    }

    void log(const string_view& str) {
        lock_guard<mutex> lock(logPrintMutex);

        const int prefixLen = formatTimePrefix();

        int len = str.length() + prefixLen;
        memcpy(lineCache + prefixLen, str.data(), str.length());

        lineCache[len++] = '\n';

        if (toFileFlag)
            toFile(lineCache, len);
        else
            toMem(lineCache, len);
    }


    template<typename... Args>
    void logFmt(const char* fmt, Args&&... args) {
        lock_guard<mutex> lock(logPrintMutex);

        const int prefixLen = formatTimePrefix();

        int len = snprintf(lineCache + prefixLen, (size_t)(LINE_SIZE - prefixLen), fmt, std::forward<Args>(args)...) + prefixLen;

        if (len <= 11 || LINE_SIZE <= (len + 1)) {
            lineCache[11] = 0;
            fprintf(stderr, "日志异常: len[%d] lineCache[%s]", len, lineCache);
            return;
        }

        lineCache[len++] = '\n';

        if (toFileFlag)
            toFile(lineCache, len);
        else
            toMem(lineCache, len);
    }

    void debug(const string_view& str) {
        if (!isDebugOn())return;

        lock_guard<mutex> lock(logPrintMutex);

        const int prefixLen = formatTimeDebug();

        int len = str.length() + prefixLen;
        memcpy(lineCache + prefixLen, str.data(), str.length());

        lineCache[len++] = '\n';

        if (toFileFlag)
            toFile(lineCache, len);
        else
            toMem(lineCache, len);
    }

    template<typename... Args>
    void debugFmt(const char* fmt, Args&&... args) {
        if (!isDebugOn())return;

        lock_guard<mutex> lock(logPrintMutex);

        const int prefixLen = formatTimeDebug();

        int len = snprintf(lineCache + prefixLen, (size_t)(LINE_SIZE - prefixLen), fmt, std::forward<Args>(args)...) + prefixLen;

        if (len <= 13 || LINE_SIZE <= (len + 1)) {
            lineCache[13] = 0;
            fprintf(stderr, "日志异常: len[%d] lineCache[%s]", len, lineCache);
            return;
        }

        lineCache[len++] = '\n';

        if (toFileFlag)
            toFile(lineCache, len);
        else
            toMem(lineCache, len);
    }

    void clearLog() {
        logCache[0] = '\n';
        position = 1;
    }

    char* getLogPtr() {
        return logCache;
    }

    size_t getLoglen() {
        return position;
    }
};
