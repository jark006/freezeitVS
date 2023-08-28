#pragma once

#include "utils.hpp"
#include "freezeit.hpp"

class Settings {
private:
    Freezeit& freezeit;
    mutex writeSettingMutex;

    string settingsPath;

    const static size_t SETTINGS_SIZE = 256;
    uint8_t settingsVar[SETTINGS_SIZE] = {
            6,  //[0] 设置文件版本
            0,  //[1] 绑定到 CPU核心簇
            10, //[2] freezeTimeout sec
            20, //[3] wakeupTimeoutMin min
            20, //[4] terminateTimeout sec
            5,  //[5] setMode
            2,  //[6] refreezeTimeout
            0,  //[7]
            0,  //[8]
            0,  //[9]
            1,  //[10] 激进前台识别
            0,  //[11]
            0,  //[12]
            1,  //[13] 电池监控
            0,  //[14] 电流校准
            0,  //[15] QQ/TIM冻结断网
            0,  //[16] 调整 lmk 参数 仅安卓11-15
            1,  //[17] 深度Doze
            0,  //[18] 扩展前台
            1,  //[19]
            0,  //[20]
            0,  //[21]
            0,  //[22]
            0,  //[13]
            0,  //[24]
            0,  //[25]
            0,  //[26]
            0,  //[27]
            0,  //[28]
            0,  //[29]
            0,  //[30] Doze调试日志
            0,  //[31]
            0,  //[32]
    };

public:
    uint8_t& settingsVer = settingsVar[0];       // 设置文件版本
    //uint8_t& unknown = settingsVar[1];       // 
    uint8_t& freezeTimeout = settingsVar[2];     // 单位 秒
    uint8_t& wakeupTimeoutMin = settingsVar[3];  // 单位 分
    uint8_t& terminateTimeout = settingsVar[4];  // 单位 秒
    uint8_t& setMode = settingsVar[5];           // Freezer模式
    uint8_t& refreezeTimeoutIdx = settingsVar[6];// 定时压制 参数索引 0-4

    uint8_t& enableBatteryMonitor = settingsVar[13];   // 电池监控
    uint8_t& enableCurrentFix = settingsVar[14];       // 电池电流校准
    //uint8_t& enableBreakNetwork = settingsVar[15];     // 
    uint8_t& enableLMK = settingsVar[16];              // 调整 lmk 参数 仅安卓11-15
    uint8_t& enableDoze = settingsVar[17];             // 深度Doze
    uint8_t& enableWindows = settingsVar[18];          // 扩展前台

    uint8_t& enableScreenDebug = settingsVar[30];        // Doze调试日志

    Settings& operator=(Settings&&) = delete;

    Settings(Freezeit& freezeit) : freezeit(freezeit) {

        settingsPath = freezeit.modulePath + "/settings.db";

        auto fd = open(settingsPath.c_str(), O_RDONLY);
        if (fd > 0) {
            uint8_t tmp[SETTINGS_SIZE] = { 0 };
            int readSize = read(fd, tmp, SETTINGS_SIZE);
            close(fd);

            if (readSize != SETTINGS_SIZE) {
                freezeit.log("设置文件校验失败, 将使用默认设置参数, 并更新设置文件");
                freezeit.logFmt("读取大小: %d Bytes.  要求大小: 256 Bytes.", readSize);
                freezeit.log(save() ? "⚙️设置成功" : "🔧设置文件写入失败");
            }
            else if (tmp[0] != settingsVer) {
                freezeit.log("设置文件版本不兼容, 将使用默认设置参数, 并更新设置文件");
                freezeit.logFmt("读取版本: V%d 要求版本: V%d", static_cast<int>(tmp[0]),
                    static_cast<int>(settingsVer));
                freezeit.log(save() ? "⚙️设置成功" : "🔧设置文件写入失败");
            }
            else {
                memcpy(settingsVar, tmp, SETTINGS_SIZE);

                bool isError = false;
                if (setMode > 5) {
                    freezeit.logFmt("冻结模式参数[%d]错误, 已重设为 全局SIGSTOP", static_cast<int>(setMode));
                    setMode = 0;
                    isError = true;
                }
                if (refreezeTimeoutIdx > 4) {
                    freezeit.logFmt("定时压制参数[%d]错误, 已重设为 30分钟", static_cast<int>(refreezeTimeoutIdx));
                    refreezeTimeoutIdx = 2;
                    isError = true;
                }
                if (freezeTimeout < 1 || freezeTimeout > 60) {
                    freezeit.logFmt("超时冻结参数[%d]错误, 已重置为10秒", static_cast<int>(freezeTimeout));
                    freezeTimeout = 10;
                    isError = true;
                }
                if (wakeupTimeoutMin < 3 || wakeupTimeoutMin > 120) {
                    freezeit.logFmt("定时解冻参数[%d]错误, 已重置为30分", static_cast<int>(wakeupTimeoutMin));
                    wakeupTimeoutMin = 30;
                    isError = true;
                }
                if (terminateTimeout < 3 || terminateTimeout > 120) {
                    freezeit.logFmt("超时杀死参数[%d]错误, 已重置为30秒", static_cast<int>(terminateTimeout));
                    terminateTimeout = 30;
                    isError = true;
                }
                if (isError)
                    freezeit.log(save() ? "⚙️设置成功" : "🔧设置文件写入失败");
            }
        }
        else {
            if (freezeit.isOppoVivo) {
                freezeit.log("开启扩展识别 OPPO/VIVO/IQOO/REALME");
                enableWindows = true;
            }
            freezeit.log("设置文件不存在, 将初始化设置文件");
            freezeit.log(save() ? "⚙️设置成功" : "🔧设置文件写入失败");
        }
    }

    uint8_t& operator[](const int key) {
        return settingsVar[key];
    }

    uint8_t* get() {
        return settingsVar;
    }

    size_t size() {
        return SETTINGS_SIZE;
    }

    int getRefreezeTimeout() {
        constexpr int timeoutList[5] = { 86400 * 365, 900, 1800, 3600, 7200 };
        return timeoutList[refreezeTimeoutIdx < 5 ? refreezeTimeoutIdx : 0];
    }

    bool save() {
        lock_guard<mutex> lock(writeSettingMutex);
        auto fd = open(settingsPath.c_str(), O_WRONLY | O_TRUNC | O_CREAT, 0666);
        if (fd > 0) {
            int writeSize = write(fd, settingsVar, SETTINGS_SIZE);
            close(fd);
            if (writeSize == SETTINGS_SIZE)
                return true;

            freezeit.logFmt("设置异常, 文件实际写入[%d]Bytes", writeSize);
        }
        return false;
    }

    int checkAndSet(const int idx, const int val, char* replyBuf) {
        const size_t REPLY_BUF_SIZE = 2048;

        switch (idx) {
        case 1: { //[1] 绑定到 CPU核心
            if (6 < val) // 0~6 核心簇
                return snprintf(replyBuf, REPLY_BUF_SIZE, "核心错误, 正常范围:0~6, 欲设为:%d", val);
        }
              break;

        case 2: { //[2] freezeTimeout sec
            if (val < 1 || 60 < val)
                return snprintf(replyBuf, REPLY_BUF_SIZE, "超时冻结参数错误, 正常范围:1~60, 欲设为:%d", val);
        }
              break;

        case 3: {  // wakeupTimeoutMin min
            if (val < 3 || 120 < val)
                return snprintf(replyBuf, REPLY_BUF_SIZE, "定时解冻参数错误, 正常范围:3~120, 欲设为:%d", val);
        }
              break;

        case 4: { // TERMINATE sec
            if (val < 3 || 120 < val)
                return snprintf(replyBuf, REPLY_BUF_SIZE, "超时杀死参数错误, 正常范围:3~120, 欲设为:%d", val);
        }
              break;

        case 5: { // setMode 0-5
            if (5 < val)
                return snprintf(replyBuf, REPLY_BUF_SIZE, "冻结模式参数错误, 正常范围:0~5, 欲设为:%d", val);
        }
              break;

        case 6: { // 定时压制
            if (5 < val)
                return snprintf(replyBuf, REPLY_BUF_SIZE, "定时压制参数错误, 正常范围:0~5, 欲设为:%d", val);
        }
              break;

        case 10: // xxx
        case 11: // xxx
        case 12: // xxx
        case 13: // 电池监控
        case 14: // 电流校准
        case 15: // QQ/TIM冻结断网
        case 16: // lmk
        case 17: // doze
        case 18: // 扩展前台
        case 19: //
        case 20: //
        case 21: //
        case 22: //
        case 23: //
        case 24: //
        case 25: //
        case 26: //
        case 27: //
        case 28: //
        case 29: //
        case 30: // Doze调试日志
        {
            if (val != 0 && val != 1)
                return snprintf(replyBuf, REPLY_BUF_SIZE, "开关值错误, 正常范围:0/1, 欲设为:%d", val);
        }
        break;

        default: {
            freezeit.logFmt("🔧设置失败，设置项不存在, [%d]:[%d]", idx, val);
            return snprintf(replyBuf, REPLY_BUF_SIZE, "设置项不存在, [%d]:[%d]", idx, val);
        }
        }

        settingsVar[idx] = val;
        if (save()) {
            freezeit.log("⚙️设置成功");
            return snprintf(replyBuf, REPLY_BUF_SIZE, "success");
        }
        else {
            freezeit.logFmt("🔧设置失败，写入设置文件失败, [%d]:%d", idx, val);
            return snprintf(replyBuf, REPLY_BUF_SIZE, "写入设置文件失败, [%d]:%d", idx, val);
        }
    }
};
