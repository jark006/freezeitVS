#pragma once

#include "utils.hpp"
#include "vpopen.hpp"
#include "managedApp.hpp"
#include "freezeit.hpp"
#include "systemTools.hpp"

class Doze {
private:
	Freezeit& freezeit;
	ManagedApp& managedApp;
	SystemTools& systemTools;
	Settings& settings;

	time_t enterDozeTimeStamp = 0;
	uint32_t enterDozeCycleStamp = 0;
	time_t lastInteractiveTime = time(nullptr); // 上次检查为 亮屏或充电 的时间戳

	void updateDozeWhitelist() {
		START_TIME_COUNT;

		const char* cmdList[] = { "/system/bin/dumpsys", "dumpsys", "deviceidle", "whitelist",
								 nullptr };
		char buf[1024 * 32];
		VPOPEN::vpopen(cmdList[0], cmdList + 1, buf, sizeof(buf));

		stringstream ss;
		ss << buf;

		string tmp, tmpLabel, line;
		set<int> existSet;

		// https://cs.android.com/android/platform/superproject/+/android-12.1.0_r27:frameworks/base/apex/jobscheduler/service/java/com/android/server/DeviceIdleController.java;l=485
		// "system-excidle,xxx,uid"  该名单在Doze模式会失效
		// "system,xxx,uid"
		// "user,xxx,uid"
		while (getline(ss, line)) {
			if (!line.starts_with("system,") && !line.starts_with("user")) continue;
			if (line.length() < 10)continue;
			if (line[line.length() - 6] != ',')continue;

			int uid = atoi(line.c_str() + line.length() - 5);
			if (managedApp.without(uid))continue;

			auto& info = managedApp.getRaw()[uid];
			if (info.freezeMode < FREEZE_MODE::WHITELIST) {
				tmp += "dumpsys deviceidle whitelist -" + info.package + ";";
				tmpLabel += info.label + " ";
			}
			else
				existSet.insert(uid);
		}

		if (tmp.length()) {
			freezeit.log("移除电池优化白名单: %s", tmpLabel.c_str());
			system(tmp.c_str());
		}

		tmp.clear();
		tmpLabel.clear();
		for (const auto& [uid, info] : managedApp.getRaw()) {
			if (info.isSystemApp) continue;

			if (info.freezeMode >= FREEZE_MODE::WHITELIST && !existSet.contains(uid)) {
				tmp += "dumpsys deviceidle whitelist +" + info.package + ";";
				tmpLabel += info.label + " ";
			}
		}
		if (tmp.length()) {
			freezeit.log("加入电池优化白名单: %s", tmpLabel.c_str());
			system(tmp.c_str());
		}

		if (settings.enableScreenDebug) {
			tmp.clear();
			for (const auto uid : existSet)
				tmp += managedApp[uid].label + " ";
			if (tmp.length())
				freezeit.log("已在白名单: %s", tmp.c_str());
		}

		END_TIME_COUNT;
	}

	// 0获取失败 1息屏 2亮屏
	int getScreenByLocalSocket() {
		START_TIME_COUNT;

		int buff[64];
		int recvLen = Utils::localSocketRequest(XPOSED_CMD::GET_SCREEN, nullptr, 0, buff,
			sizeof(buff));

		if (recvLen == 0) {
			freezeit.log("%s() 工作异常, 请确认LSPosed中冻它勾选系统框架, 然后重启", __FUNCTION__);
			END_TIME_COUNT;
			return 0;
		}
		else if (recvLen != 4) {
			freezeit.log("%s() 屏幕数据异常 recvLen[%d]", __FUNCTION__, recvLen);
			if (recvLen > 0 && recvLen < 64 * 4)
				freezeit.log("DumpHex: [%s]", Utils::bin2Hex(buff, recvLen).c_str());
			END_TIME_COUNT;
			return 0;
		}

		if (settings.enableScreenDebug) {
			const char* str[3] = { "Doze调试: Xposed 获取屏幕状态失败",
								  "Doze调试: Xposed 息屏中",
								  "Doze调试: Xposed 亮屏中" };
			freezeit.log(str[buff[0] < 3 ? buff[0] : 1]);
		}


		END_TIME_COUNT;
		return buff[0];
	}

	bool isInteractive() {
		/*
		[debug.tracing.screen_brightness]: [0.05468459]   0-1 / 0-16384
		[debug.tracing.screen_state]: [2]  亮屏[2] 息屏[1]
		https://cs.android.com/android/platform/superproject/+/master:frameworks/base/core/java/android/view/Display.java;l=387
		enum DisplayStateEnum
		public static final int DISPLAY_STATE_UNKNOWN = 0;
		public static final int DISPLAY_STATE_OFF = 1;
		public static final int DISPLAY_STATE_ON = 2;
		public static final int DISPLAY_STATE_DOZE = 3; //亮屏但处于Doze的非交互状态状态
		public static final int DISPLAY_STATE_DOZE_SUSPEND = 4; // 同上，但CPU不控制显示，由协处理器或其他控制
		public static final int DISPLAY_STATE_VR = 5;
		public static final int DISPLAY_STATE_ON_SUSPEND = 6; //非Doze, 类似4
		*/
		do {
			char res[128]; // MAX LEN: 96
			int mScreenState;
			if (__system_property_get("debug.tracing.screen_state", res) < 1)
				mScreenState = getScreenByLocalSocket();
			else mScreenState = res[0] - '0';

			if (settings.enableScreenDebug)
				if (mScreenState != 1 && mScreenState != 2)
					freezeit.log("Doze调试: 屏幕其他状态 mScreenState[%d]", mScreenState);

			if (mScreenState == 2 || mScreenState == 5 || mScreenState == 6) {
				if (settings.enableScreenDebug)
					freezeit.log("Doze调试: 亮屏中 mScreenState[%d]", mScreenState);
				break;
			}

			if (mScreenState <= 0) {
				freezeit.log("屏幕状态获取失败 mScreenState[%d]", mScreenState);
				break;
			}

			// 以下则是息屏: 1 3 4

			if (systemTools.isAudioPlaying) {
				if (settings.enableScreenDebug)
					freezeit.log("Doze调试: 息屏, 播放中");
				break;
			}

			// "Unknown", "Charging", "Discharging", "Not charging", "Full"
			// https://cs.android.com/android/kernel/superproject/+/common-android-mainline-kleaf:common/drivers/power/supply/power_supply_sysfs.c;l=75
			Utils::readString("/sys/class/power_supply/battery/status", res, sizeof(res));
			if (!strncmp(res, "Charging", 4) || !strncmp(res, "Full", 4)) {
				if (settings.enableScreenDebug)
					freezeit.log("Doze调试: 息屏, 充电中");
				break;
			}

			if (!strncmp(res, "Discharging", 4) || !strncmp(res, "Not charging", 4)) {
				if (settings.enableScreenDebug)
					freezeit.log("Doze调试: 息屏, 未充电");
				return false;
			}

			if (settings.enableScreenDebug)
				freezeit.log("Doze调试: 息屏, 电池状态未知 [%s]", res);

		} while (false);

		lastInteractiveTime = time(nullptr);
		return true;
	}

public:
	Doze& operator=(Doze&&) = delete;

	bool isScreenOffStandby = false;

	Doze(Freezeit& freezeit, Settings& settings, ManagedApp& managedApp, SystemTools& systemTools) :
		freezeit(freezeit), managedApp(managedApp), systemTools(systemTools), settings(settings) {
		updateUidTime();
	}

	bool checkIfNeedToExit() {
		START_TIME_COUNT;
		if (!isInteractive()) {
			if (settings.enableScreenDebug)
				freezeit.log("Doze调试: 息屏中, 发现有活动");

			END_TIME_COUNT;
			return false;
		}

		isScreenOffStandby = false;

		if (settings.enableDoze) {
			system("dumpsys deviceidle unforce");

			int deltaTime = time(nullptr) - enterDozeTimeStamp;
			const int activeRate =
				deltaTime > 0 ? (1000 * (systemTools.cycleCnt - enterDozeCycleStamp) /
					deltaTime) : 0; //CPU 活跃率

			if (deltaTime < 60 || activeRate > 800)
				freezeit.log("休眠了个寂寞...");

			string tmp{ "🤪 退出深度Doze 时长 " };
			if (deltaTime >= 3600) {
				tmp += to_string(deltaTime / 3600) + "时";
				deltaTime %= 3600;
			}
			if (deltaTime >= 60) {
				tmp += to_string(deltaTime / 60) + "分";
				deltaTime %= 60;
			}
			if (deltaTime) tmp += to_string(deltaTime) + "秒";
			tmp += " 唤醒率 %d.%d %%";
			freezeit.log(tmp.c_str(), activeRate / 10, activeRate % 10);

			char buf[1024 * 16];
			size_t len = 0;

			struct st {
				int uid;
				int delta;
			};
			vector<st> uidTimeSort;
			uidTimeSort.reserve(32);
			for (const auto& [uid, timeList] : updateUidTime()) {
				int delta = (timeList.total - timeList.lastTotal); // 毫秒
				if (delta <= 100)continue; // 过滤 100毫秒
				uidTimeSort.emplace_back(st{ uid, delta });
			}

			std::sort(uidTimeSort.begin(), uidTimeSort.end(),
				[](const st& a, const st& b) { return a.delta > b.delta; });

			for (auto& [uid, delta] : uidTimeSort) {
				STRNCAT(buf, len, "[");
				if (delta > (60 * 1000)) {
					STRNCAT(buf, len, "%d分", delta / (60 * 1000));
					delta %= (60 * 1000);
				}
				STRNCAT(buf, len, "%d.%03d秒] ", delta / 1000, delta % 1000);
				STRNCAT(buf, len, "%s\n", managedApp.getLabel(uid).c_str());
			}


			if (len)
				freezeit.log("Doze期间应用的CPU活跃时间:\n\n%s", buf);
		}
		END_TIME_COUNT;
		return true;
	}

	bool checkIfNeedToEnter() {
		constexpr int TIMEOUT = 3 * 60;
		static int secCnt = 30;

		if (isScreenOffStandby || ++secCnt < TIMEOUT)
			return false;

		secCnt = 0;

		if (isInteractive())
			return false;

		const time_t nowTimeStamp = time(nullptr);
		if ((nowTimeStamp - lastInteractiveTime) < (TIMEOUT + 60L))
			return false;

		if (settings.enableScreenDebug)
			freezeit.log("息屏状态已超时，正在确认息屏状态");

		// 如果系统之前已经自行进入轻度Doze, 退出Doze的瞬间（此时可能还没亮屏）导致现在才执行时间判断
		// 此时进入Doze不合理，需等等，再确认一遍
		usleep(1000 * 200); // 休眠 200ms
		if (isInteractive()) {
			if (settings.enableScreenDebug)
				freezeit.log("确认新状态：已亮屏或充电中, 退出息屏");
			return false;
		}

		isScreenOffStandby = true;

		if (settings.enableDoze) {
			if (settings.enableScreenDebug)
				freezeit.log("开始准备深度Doze");
			updateDozeWhitelist();
			updateUidTime();

			freezeit.log("😴 进入深度Doze");
			enterDozeTimeStamp = nowTimeStamp;
			enterDozeCycleStamp = systemTools.cycleCnt;

			system(
				"dumpsys deviceidle enable all;"
				"dumpsys deviceidle force-idle deep"
			);
		}
		return true;
	}


	map<int, uidTimeStruct> uidTime; // ms 微秒
	map<int, uidTimeStruct>& updateUidTime() {

		START_TIME_COUNT;

		stringstream ss;
		ss << ifstream("/proc/uid_cputime/show_uid_stat").rdbuf();

		string line;
		while (getline(ss, line)) {
			int uid;
			long long userTime, systemTime; // us 微秒
			sscanf(line.c_str(), "%d: %lld %lld", &uid, &userTime, &systemTime);
			if (managedApp.contains(uid) && (userTime >= 1000 || systemTime >= 1000)) {
				auto& appTime = uidTime[uid];
				appTime.lastTotal = appTime.total;
				appTime.total = static_cast<int>((systemTime + userTime) / 1000);  // ms 取毫秒
			}
		}

		END_TIME_COUNT;
		return uidTime;
	}
};
