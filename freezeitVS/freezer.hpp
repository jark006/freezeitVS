#pragma once

#include "utils.hpp"
#include "vpopen.hpp"
#include "managedApp.hpp"
#include "doze.hpp"
#include "freezeit.hpp"
#include "systemTools.hpp"

class Freezer {
private:
	Freezeit& freezeit;
	ManagedApp& managedApp;
	SystemTools& systemTools;
	Settings& settings;
	Doze& doze;

	vector<thread> threads;

	WORK_MODE workMode = WORK_MODE::GLOBAL_SIGSTOP;
	map<int, int> pendingHandleList;     //挂起列队 无论黑白名单 { uid, timeRemain:sec }
	set<int> lastForegroundApp;          //前台应用
	set<int> curForegroundApp;           //新前台应用
	set<int> curFgBackup;                //新前台应用备份 用于进入doze前备份， 退出后恢复

	uint32_t timelineIdx = 0;
	uint32_t unfrozenTimeline[4096] = {};
	map<int, uint32_t> unfrozenIdx;

	int refreezeSecRemain = 60; //开机 一分钟时 就压一次
	int remainTimesToRefreshTopApp = 2; //允许多线程冲突，不需要原子操作

	static const size_t GET_VISIBLE_BUF_SIZE = 256 * 1024;
	unique_ptr<char[]> getVisibleAppBuff;

	struct binder_state {
		int fd = -1;
		void* mapped = nullptr;
		size_t mapSize = 128 * 1024;
	} bs;

	const char* cgroupV2FreezerCheckPath = "/sys/fs/cgroup/uid_0/cgroup.freeze";
	const char* cgroupV2frozenCheckPath = "/sys/fs/cgroup/frozen/cgroup.freeze";       // "1" frozen
	const char* cgroupV2unfrozenCheckPath = "/sys/fs/cgroup/unfrozen/cgroup.freeze";   // "0" unfrozen

	// const char cpusetEventPath[] = "/dev/cpuset/top-app";
	const char* cpusetEventPathA12 = "/dev/cpuset/top-app/tasks";
	const char* cpusetEventPathA13 = "/dev/cpuset/top-app/cgroup.procs";

	// const char* cgroupV1UidPath = "/dev/jark_freezer/uid_%d";
	const char* cgroupV1FrozenPath = "/dev/jark_freezer/frozen/cgroup.procs";
	const char* cgroupV1UnfrozenPath = "/dev/jark_freezer/unfrozen/cgroup.procs";

	// 如果直接使用 uid_xxx/cgroup.freeze 可能导致无法解冻
	const char* cgroupV2UidPidPath = "/sys/fs/cgroup/uid_%d/pid_%d/cgroup.freeze"; // "1"frozen   "0"unfrozen
	const char* cgroupV2FrozenPath = "/sys/fs/cgroup/frozen/cgroup.procs";         // write pid
	const char* cgroupV2UnfrozenPath = "/sys/fs/cgroup/unfrozen/cgroup.procs";     // write pid


	const char v2wchan[16] = "do_freezer_trap";      // FreezerV2冻结状态
	const char v1wchan[15] = "__refrigerator";       // FreezerV1冻结状态
	const char SIGSTOPwchan[15] = "do_signal_stop";  // SIGSTOP冻结状态
	const char v2xwchan[11] = "get_signal";          //不完整V2冻结状态
	// const char epoll_wait1_wchan[] = "SyS_epoll_wait";
	// const char epoll_wait2_wchan[] = "do_epoll_wait";
	// const char binder_wchan[] = "binder_ioctl_write_read";
	// const char pipe_wchan[] = "pipe_wait";

public:
	Freezer& operator=(Freezer&&) = delete;

	const string workModeStr(WORK_MODE mode) {
		const string modeStrList[] = {
				"全局SIGSTOP",
				"FreezerV1 (FROZEN)",
				"FreezerV1 (FRZ+ST)",
				"FreezerV2 (UID)",
				"FreezerV2 (FROZEN)",
				"Unknown" };
		const uint32_t idx = static_cast<uint32_t>(mode);
		return modeStrList[idx <= 5 ? idx : 5];
	}

	Freezer(Freezeit& freezeit, Settings& settings, ManagedApp& managedApp,
		SystemTools& systemTools, Doze& doze) :
		freezeit(freezeit), managedApp(managedApp), systemTools(systemTools),
		settings(settings), doze(doze) {

		getVisibleAppBuff = make_unique<char[]>(GET_VISIBLE_BUF_SIZE);

		if (freezeit.kernelVersion.main >= 5 && freezeit.kernelVersion.sub >= 10) {
			const int res = binder_open("/dev/binder");
			if (res > 0)
				freezeit.log("初始驱动 BINDER协议版本 %d", res);
			else
				freezeit.log("初始驱动 BINDER失败");
		}

		threads.emplace_back(thread(&Freezer::cpuSetTriggerTask, this)); //监控前台
		threads.emplace_back(thread(&Freezer::cycleThreadFunc, this));

		checkAndMountV2();
		switch (static_cast<WORK_MODE>(settings.setMode)) {
		case WORK_MODE::GLOBAL_SIGSTOP: {
			workMode = WORK_MODE::GLOBAL_SIGSTOP;
			freezeit.setWorkMode(workModeStr(workMode));
			freezeit.log("已设置[全局SIGSTOP], [Freezer冻结]将变为[SIGSTOP冻结]");
		}
									  return;

		case WORK_MODE::V1F: {
			if (mountFreezerV1()) {
				workMode = WORK_MODE::V1F;
				freezeit.setWorkMode(workModeStr(workMode));
				freezeit.log("Freezer类型已设为 V1(FROZEN)");
				return;
			}
			freezeit.log("不支持自定义Freezer类型 V1(FROZEN) 失败");
		}
						   break;

		case WORK_MODE::V1F_ST: {
			if (mountFreezerV1()) {
				workMode = WORK_MODE::V1F_ST;
				freezeit.setWorkMode(workModeStr(workMode));
				freezeit.log("Freezer类型已设为 V1(FRZ+ST)");
				return;
			}
			freezeit.log("不支持自定义Freezer类型 V1(FRZ+ST)");
		}
							  break;

		case WORK_MODE::V2UID: {
			if (checkFreezerV2UID()) {
				workMode = WORK_MODE::V2UID;
				freezeit.setWorkMode(workModeStr(workMode));
				freezeit.log("Freezer类型已设为 V2(UID)");
				return;
			}
			freezeit.log("不支持自定义Freezer类型 V2(UID)");
		}
							 break;

		case WORK_MODE::V2FROZEN: {
			if (checkFreezerV2FROZEN()) {
				workMode = WORK_MODE::V2FROZEN;
				freezeit.setWorkMode(workModeStr(workMode));
				freezeit.log("Freezer类型已设为 V2(FROZEN)");
				return;
			}
			freezeit.log("不支持自定义Freezer类型 V2(FROZEN)");
		}
								break;
		}

		if (checkFreezerV2FROZEN()) {
			workMode = WORK_MODE::V2FROZEN;
			freezeit.log("Freezer类型已设为 V2(FROZEN)");
		}
		else if (checkFreezerV2UID()) {
			workMode = WORK_MODE::V2UID;
			freezeit.log("Freezer类型已设为 V2(UID)");
		}
		else if (mountFreezerV1()) {
			workMode = WORK_MODE::V1F;
			freezeit.log("Freezer类型已设为 V1(FROZEN)");
		}
		else {
			workMode = WORK_MODE::GLOBAL_SIGSTOP;
			freezeit.log("不支持任何Freezer, 已开启 [全局SIGSTOP] 冻结模式");
		}
		freezeit.setWorkMode(workModeStr(workMode));
	}

	bool isV1Mode() {
		return workMode == WORK_MODE::V1F_ST || workMode == WORK_MODE::V1F;
	}

	void getPids(appInfoStruct& info, const int uid) {
		START_TIME_COUNT;

		info.pids.clear();

		DIR* dir = opendir("/proc");
		if (dir == nullptr) {
			char errTips[256];
			snprintf(errTips, sizeof(errTips), "错误: %s() [%d]:[%s]", __FUNCTION__, errno,
				strerror(errno));
			fprintf(stderr, "%s", errTips);
			freezeit.log(errTips);
			return;
		}

		struct dirent* file;
		while ((file = readdir(dir)) != nullptr) {
			if (file->d_type != DT_DIR) continue;
			if (file->d_name[0] < '0' || file->d_name[0] > '9') continue;

			const int pid = atoi(file->d_name);
			if (pid <= 100) continue;

			char fullPath[64];
			memcpy(fullPath, "/proc/", 6);
			memcpy(fullPath + 6, file->d_name, 6);

			struct stat statBuf;
			if (stat(fullPath, &statBuf))continue;
			if (statBuf.st_uid != (uid_t)uid) continue;

			strcat(fullPath + 8, "/cmdline");
			char readBuff[256];
			if (Utils::readString(fullPath, readBuff, sizeof(readBuff)) == 0)continue;
			const string& package = info.package;
			if (strncmp(readBuff, package.c_str(), package.length())) continue;
			const char endChar = readBuff[package.length()];
			if (endChar != ':' && endChar != 0)continue;

			info.pids.emplace_back(pid);
		}
		closedir(dir);
		END_TIME_COUNT;
	}

	map<int, vector<int>> getRunningPids(set<int>& uidSet) {
		START_TIME_COUNT;
		map<int, vector<int>> pids;

		DIR* dir = opendir("/proc");
		if (dir == nullptr) {
			char errTips[256];
			snprintf(errTips, sizeof(errTips), "错误: %s() [%d]:[%s]", __FUNCTION__, errno,
				strerror(errno));
			fprintf(stderr, "%s", errTips);
			freezeit.log(errTips);
			return pids;
		}

		struct dirent* file;
		while ((file = readdir(dir)) != nullptr) {
			if (file->d_type != DT_DIR) continue;
			if (file->d_name[0] < '0' || file->d_name[0] > '9') continue;

			const int pid = atoi(file->d_name);
			if (pid <= 100) continue;

			char fullPath[64];
			memcpy(fullPath, "/proc/", 6);
			memcpy(fullPath + 6, file->d_name, 6);

			struct stat statBuf;
			if (stat(fullPath, &statBuf))continue;
			const int uid = statBuf.st_uid;
			if (!uidSet.contains(uid))continue;

			strcat(fullPath + 8, "/cmdline");
			char readBuff[256];
			if (Utils::readString(fullPath, readBuff, sizeof(readBuff)) == 0)continue;
			const string& package = managedApp[uid].package;
			if (strncmp(readBuff, package.c_str(), package.length())) continue;

			pids[uid].emplace_back(pid);
		}
		closedir(dir);
		END_TIME_COUNT;
		return pids;
	}

	[[maybe_unused]] set<int> getRunningUids(set<int>& uidSet) {
		START_TIME_COUNT;
		set<int> uids;

		DIR* dir = opendir("/proc");
		if (dir == nullptr) {
			char errTips[256];
			snprintf(errTips, sizeof(errTips), "错误: %s() [%d]:[%s]", __FUNCTION__, errno,
				strerror(errno));
			fprintf(stderr, "%s", errTips);
			freezeit.log(errTips);
			return uids;
		}

		struct dirent* file;
		while ((file = readdir(dir)) != nullptr) {
			if (file->d_type != DT_DIR) continue;
			if (file->d_name[0] < '0' || file->d_name[0] > '9') continue;

			const int pid = atoi(file->d_name);
			if (pid <= 100) continue;

			char fullPath[64];
			memcpy(fullPath, "/proc/", 6);
			memcpy(fullPath + 6, file->d_name, 6);

			struct stat statBuf;
			if (stat(fullPath, &statBuf))continue;
			const int uid = statBuf.st_uid;
			if (!uidSet.contains(uid))continue;

			strcat(fullPath + 8, "/cmdline");
			char readBuff[256];
			if (Utils::readString(fullPath, readBuff, sizeof(readBuff)) == 0)continue;
			const string& package = managedApp[uid].package;
			if (strncmp(readBuff, package.c_str(), package.length())) continue;

			uids.insert(uid);
		}
		closedir(dir);
		END_TIME_COUNT;
		return uids;
	}

	void handleSignal(const int uid, const vector<int>& pids, const int signal) {
		if (signal == SIGKILL) { //先暂停 然后再杀，否则有可能会复活
			for (const auto pid : pids)
				kill(pid, SIGSTOP);
			usleep(1000 * 100);
		}

		for (const int pid : pids)
			if (kill(pid, signal) < 0 && (signal == SIGSTOP || signal == SIGKILL))
				freezeit.log("%s [%s PID:%d] 失败(SIGSTOP):%s", signal == SIGSTOP ? "冻结" : "杀死",
					managedApp[uid].label.c_str(), pid, strerror(errno));
	}

	void handleFreezer(const int uid, const vector<int>& pids, const int signal) {
		char path[256];

		switch (workMode) {
		case WORK_MODE::V2FROZEN: {
			for (const int pid : pids) {
				if (!Utils::writeInt(
					signal == SIGSTOP ? cgroupV2FrozenPath : cgroupV2UnfrozenPath, pid))
					freezeit.log("%s [%s PID:%d] 失败(V2FROZEN)",
						(signal == SIGSTOP ? "冻结" : "解冻"),
						managedApp[uid].label.c_str(), pid);
			}
		}
								break;

		case WORK_MODE::V2UID: {
			for (const int pid : pids) {
				snprintf(path, sizeof(path), cgroupV2UidPidPath, uid, pid);
				if (!Utils::writeString(path, signal == SIGSTOP ? "1" : "0", 2))
					freezeit.log("%s [%s PID:%d] 失败(进程可能已结束或者Freezer控制器尚未初始化PID路径)",
						(signal == SIGSTOP ? "冻结" : "解冻"),
						managedApp[uid].label.c_str(), pid);
			}
			//                snprintf(path, sizeof(path), cgroupV2UidPath, uid);
			//                if (!Utils::writeString(path, signal == SIGSTOP ? "1" : "0", 2))
			//                    freezeit.log("%s %s 失败(进程可能已结束或者Freezer控制器尚未初始化UID路径)",
			//                                 (signal == SIGSTOP ? "冻结" : "解冻"),
			//                                 managedApp[uid].label.c_str());
		}
							 break;

		case WORK_MODE::V1F_ST: {
			if (signal == SIGSTOP) {
				for (const int pid : pids) {
					if (!Utils::writeInt(cgroupV1FrozenPath, pid))
						freezeit.log("冻结 [%s PID:%d] 失败(V1F_ST_F)",
							managedApp[uid].label.c_str(), pid);
					if (kill(pid, signal) < 0)
						freezeit.log("冻结 [%s PID:%d] 失败(V1F_ST_S)",
							managedApp[uid].label.c_str(), pid);
				}
			}
			else {
				for (const int pid : pids) {
					if (kill(pid, signal) < 0)
						freezeit.log("解冻 [%s PID:%d] 失败(V1F_ST_S)",
							managedApp[uid].label.c_str(), pid);
					if (!Utils::writeInt(cgroupV1UnfrozenPath, pid))
						freezeit.log("解冻 [%s PID:%d] 失败(V1F_ST_F)",
							managedApp[uid].label.c_str(), pid);
				}
			}
		}
							  break;

		case WORK_MODE::V1F: {
			for (const int pid : pids) {
				if (!Utils::writeInt(
					signal == SIGSTOP ? cgroupV1FrozenPath : cgroupV1UnfrozenPath, pid))
					freezeit.log("%s [%s] 失败(V1F) PID:%d", (signal == SIGSTOP ? "冻结" : "解冻"),
						managedApp[uid].label.c_str(), pid);
			}
		}
						   break;

						   // 本函数只处理Freezer模式，其他冻结模式不应来到此处
		default: {
			freezeit.log("%s 使用了错误的冻结模式", managedApp[uid].label.c_str());
		}
			   break;
		}
	}

	// 只接受 SIGSTOP SIGCONT
	int handleProcess(appInfoStruct& info, const int uid, const int signal) {
		START_TIME_COUNT;

		if (signal == SIGSTOP)
			getPids(info, uid);
		else if (signal == SIGCONT) {
			erase_if(info.pids, [](const int& pid) {
				char path[16];
				snprintf(path, sizeof(path), "/proc/%d", pid);
				return access(path, F_OK);
				});
		}
		else {
			freezeit.log("错误执行 %s %d", info.label.c_str(), signal);
			return 0;
		}

		switch (info.freezeMode) {
		case FREEZE_MODE::FREEZER: {
			if (workMode != WORK_MODE::GLOBAL_SIGSTOP) {
				const int res = handleBinder(info.pids, signal);
				if (res < 0 && signal == SIGSTOP && info.isTolerant)
					return res;
				handleFreezer(uid, info.pids, signal);
				break;
			}
			// 如果是全局 WORK_MODE::GLOBAL_SIGSTOP 则顺着执行下面
		}

		case FREEZE_MODE::SIGNAL: {
			const int res = handleBinder(info.pids, signal);
			if (res < 0 && signal == SIGSTOP && info.isTolerant)
				return res;
			handleSignal(uid, info.pids, signal);
		}
								break;

		case FREEZE_MODE::TERMINATE: {
			if (signal == SIGSTOP)
				handleSignal(uid, info.pids, SIGKILL);
			return 0;
		}

		default: {
			freezeit.log("不再冻结此应用：%s %s", info.label.c_str(),
				getModeText(info.freezeMode).c_str());
			return 0;
		}
		}

		if (settings.wakeupTimeoutMin != 120) {
			// 无论冻结还是解冻都要清除 解冻时间线上已设置的uid
			auto it = unfrozenIdx.find(uid);
			if (it != unfrozenIdx.end())
				unfrozenTimeline[it->second] = 0;

			// 冻结就需要在 解冻时间线 插入下一次解冻的时间
			if (signal == SIGSTOP && info.pids.size() &&
				info.freezeMode != FREEZE_MODE::TERMINATE) {
				uint32_t nextIdx =
					(timelineIdx + settings.wakeupTimeoutMin * 60) & 0x0FFF; // [ %4096]
				unfrozenIdx[uid] = nextIdx;
				unfrozenTimeline[nextIdx] = uid;
			}
			else {
				unfrozenIdx.erase(uid);
			}
		}

		if (settings.enableBreakNetwork && signal == SIGSTOP &&
			info.freezeMode != FREEZE_MODE::TERMINATE) {
			auto& package = info.package;
			if (package == "com.tencent.mobileqq" || package == "com.tencent.tim") {
				const auto ret = systemTools.breakNetworkByLocalSocket(uid);
				switch (static_cast<REPLY>(ret)) {
				case REPLY::SUCCESS:
					freezeit.log("断网成功: %s", info.label.c_str());
					break;
				case REPLY::FAILURE:
					freezeit.log("断网失败: %s", info.label.c_str());
					break;
				default:
					freezeit.log("断网 未知回应[%d] %s", ret, info.label.c_str());
					break;
				}
			}
		}

		END_TIME_COUNT;
		return info.pids.size();
	}

	// 重新压制第三方。 白名单, 前台, 待冻结列队 都跳过
	void checkReFreeze() {
		START_TIME_COUNT;

		if (--refreezeSecRemain > 0) return;

		refreezeSecRemain = settings.getRefreezeTimeout();

		map<int, vector<int>> terminateList, SIGSTOPList, freezerList;

		DIR* dir = opendir("/proc");
		if (dir == nullptr) {
			char errTips[256];
			snprintf(errTips, sizeof(errTips), "错误: %s() [%d]:[%s]", __FUNCTION__, errno,
				strerror(errno));
			fprintf(stderr, "%s", errTips);
			freezeit.log(errTips);
			return;
		}

		struct dirent* file;
		while ((file = readdir(dir)) != nullptr) {
			if (file->d_type != DT_DIR) continue;
			if (file->d_name[0] < '0' || file->d_name[0] > '9') continue;

			int pid = atoi(file->d_name);
			if (pid <= 100) continue;

			char fullPath[64];
			memcpy(fullPath, "/proc/", 6);
			memcpy(fullPath + 6, file->d_name, 6);

			struct stat statBuf;
			if (stat(fullPath, &statBuf))continue;
			const int uid = statBuf.st_uid;
			if (managedApp.without(uid)) continue;

			auto& info = managedApp[uid];
			if (info.freezeMode >= FREEZE_MODE::WHITELIST || pendingHandleList.contains(uid) ||
				curForegroundApp.contains(uid))
				continue;

			strcat(fullPath + 8, "/cmdline");
			char readBuff[256];
			if (Utils::readString(fullPath, readBuff, sizeof(readBuff)) == 0)continue;
			if (strncmp(readBuff, info.package.c_str(), info.package.length())) continue;

			switch (info.freezeMode) {
			case FREEZE_MODE::TERMINATE:
				terminateList[uid].emplace_back(pid);
				break;
			case FREEZE_MODE::FREEZER:
				if (workMode != WORK_MODE::GLOBAL_SIGSTOP) {
					freezerList[uid].emplace_back(pid);
					break;
				}
			case FREEZE_MODE::SIGNAL:
			default:
				SIGSTOPList[uid].emplace_back(pid);
				break;
			}
		}
		closedir(dir);

		vector<int> uidOfQQTIM;
		string tmp;
		for (const auto& [uid, pids] : freezerList) {
			auto& info = managedApp[uid];
			tmp += ' ';
			tmp += info.label;
			handleFreezer(uid, pids, SIGSTOP);
			managedApp[uid].pids = move(pids);

			if (settings.enableBreakNetwork &&
				(info.package == "com.tencent.mobileqq" || info.package == "com.tencent.tim"))
				uidOfQQTIM.emplace_back(uid);
		}
		if (tmp.length()) freezeit.log("定时Freezer压制: %s", tmp.c_str());

		tmp.clear();
		for (auto& [uid, pids] : SIGSTOPList) {
			auto& info = managedApp[uid];
			tmp += ' ';
			tmp += info.label;
			handleSignal(uid, pids, SIGSTOP);
			managedApp[uid].pids = move(pids);

			if (settings.enableBreakNetwork &&
				(info.package == "com.tencent.mobileqq" || info.package == "com.tencent.tim"))
				uidOfQQTIM.emplace_back(uid);
		}
		if (tmp.length()) freezeit.log("定时SIGSTOP压制: %s", tmp.c_str());

		tmp.clear();
		for (const auto& [uid, pids] : terminateList) {
			tmp += ' ';
			tmp += managedApp[uid].label;
			handleSignal(uid, pids, SIGKILL);
		}
		if (tmp.length()) freezeit.log("定时压制 杀死后台: %s", tmp.c_str());

		for (const int uid : uidOfQQTIM) {
			usleep(1000 * 100);
			systemTools.breakNetworkByLocalSocket(uid);
			freezeit.log("定时压制 断网 [%s]", managedApp[uid].label.c_str());
		}

		END_TIME_COUNT;
	}

	bool mountFreezerV1() {
		if (!access("/dev/jark_freezer", F_OK)) // 已挂载
			return true;

		// https://man7.org/linux/man-pages/man7/cgroups.7.html
		// https://www.kernel.org/doc/Documentation/cgroup-v1/freezer-subsystem.txt
		// https://www.containerlabs.kubedaily.com/LXC/Linux%20Containers/The-cgroup-freezer-subsystem.html
/*
 def infoEncrypt():
	bytes1 =\
	"mkdir /dev/jark_freezer;"\
	"mount -t cgroup -o freezer freezer /dev/jark_freezer;"\
	"sleep 1;"\
	"mkdir -p /dev/jark_freezer/frozen;"\
	"mkdir -p /dev/jark_freezer/unfrozen;"\
	"sleep 1;"\
	"echo FROZEN > /dev/jark_freezer/frozen/freezer.state;"\
	"echo THAWED > /dev/jark_freezer/unfrozen/freezer.state;"\
	"echo 1 > /dev/jark_freezer/notify_on_release;"\
	"sleep 1;".encode('utf-8')
	bytes2 = bytes(b^0x91 for b in bytes1)
	print('uint8_t tipsFormat[{}] = {{  //  ^0x91'.format(len(bytes2)+1), end='')
	cnt = 0
	for byte in bytes2:
		if cnt % 16 == 0:
			print('\n    ', end='')
		cnt += 1
		print("0x%02x, " % byte, end='')
	print('\n    0x91,  //特殊结束符\n};')
	bytes3 = bytes(b ^ 0x91 for b in bytes2)
	print('[', bytes3.decode('utf-8'), ']')
infoEncrypt()
*/
		uint8_t cmd[] = {  //  ^0x91
				0xfc, 0xfa, 0xf5, 0xf8, 0xe3, 0xb1, 0xbe, 0xf5, 0xf4, 0xe7, 0xbe, 0xfb, 0xf0, 0xe3,
				0xfa, 0xce,
				0xf7, 0xe3, 0xf4, 0xf4, 0xeb, 0xf4, 0xe3, 0xaa, 0xfc, 0xfe, 0xe4, 0xff, 0xe5, 0xb1,
				0xbc, 0xe5,
				0xb1, 0xf2, 0xf6, 0xe3, 0xfe, 0xe4, 0xe1, 0xb1, 0xbc, 0xfe, 0xb1, 0xf7, 0xe3, 0xf4,
				0xf4, 0xeb,
				0xf4, 0xe3, 0xb1, 0xf7, 0xe3, 0xf4, 0xf4, 0xeb, 0xf4, 0xe3, 0xb1, 0xbe, 0xf5, 0xf4,
				0xe7, 0xbe,
				0xfb, 0xf0, 0xe3, 0xfa, 0xce, 0xf7, 0xe3, 0xf4, 0xf4, 0xeb, 0xf4, 0xe3, 0xaa, 0xe2,
				0xfd, 0xf4,
				0xf4, 0xe1, 0xb1, 0xa0, 0xaa, 0xfc, 0xfa, 0xf5, 0xf8, 0xe3, 0xb1, 0xbc, 0xe1, 0xb1,
				0xbe, 0xf5,
				0xf4, 0xe7, 0xbe, 0xfb, 0xf0, 0xe3, 0xfa, 0xce, 0xf7, 0xe3, 0xf4, 0xf4, 0xeb, 0xf4,
				0xe3, 0xbe,
				0xf7, 0xe3, 0xfe, 0xeb, 0xf4, 0xff, 0xaa, 0xfc, 0xfa, 0xf5, 0xf8, 0xe3, 0xb1, 0xbc,
				0xe1, 0xb1,
				0xbe, 0xf5, 0xf4, 0xe7, 0xbe, 0xfb, 0xf0, 0xe3, 0xfa, 0xce, 0xf7, 0xe3, 0xf4, 0xf4,
				0xeb, 0xf4,
				0xe3, 0xbe, 0xe4, 0xff, 0xf7, 0xe3, 0xfe, 0xeb, 0xf4, 0xff, 0xaa, 0xe2, 0xfd, 0xf4,
				0xf4, 0xe1,
				0xb1, 0xa0, 0xaa, 0xf4, 0xf2, 0xf9, 0xfe, 0xb1, 0xd7, 0xc3, 0xde, 0xcb, 0xd4, 0xdf,
				0xb1, 0xaf,
				0xb1, 0xbe, 0xf5, 0xf4, 0xe7, 0xbe, 0xfb, 0xf0, 0xe3, 0xfa, 0xce, 0xf7, 0xe3, 0xf4,
				0xf4, 0xeb,
				0xf4, 0xe3, 0xbe, 0xf7, 0xe3, 0xfe, 0xeb, 0xf4, 0xff, 0xbe, 0xf7, 0xe3, 0xf4, 0xf4,
				0xeb, 0xf4,
				0xe3, 0xbf, 0xe2, 0xe5, 0xf0, 0xe5, 0xf4, 0xaa, 0xf4, 0xf2, 0xf9, 0xfe, 0xb1, 0xc5,
				0xd9, 0xd0,
				0xc6, 0xd4, 0xd5, 0xb1, 0xaf, 0xb1, 0xbe, 0xf5, 0xf4, 0xe7, 0xbe, 0xfb, 0xf0, 0xe3,
				0xfa, 0xce,
				0xf7, 0xe3, 0xf4, 0xf4, 0xeb, 0xf4, 0xe3, 0xbe, 0xe4, 0xff, 0xf7, 0xe3, 0xfe, 0xeb,
				0xf4, 0xff,
				0xbe, 0xf7, 0xe3, 0xf4, 0xf4, 0xeb, 0xf4, 0xe3, 0xbf, 0xe2, 0xe5, 0xf0, 0xe5, 0xf4,
				0xaa, 0xf4,
				0xf2, 0xf9, 0xfe, 0xb1, 0xa0, 0xb1, 0xaf, 0xb1, 0xbe, 0xf5, 0xf4, 0xe7, 0xbe, 0xfb,
				0xf0, 0xe3,
				0xfa, 0xce, 0xf7, 0xe3, 0xf4, 0xf4, 0xeb, 0xf4, 0xe3, 0xbe, 0xff, 0xfe, 0xe5, 0xf8,
				0xf7, 0xe8,
				0xce, 0xfe, 0xff, 0xce, 0xe3, 0xf4, 0xfd, 0xf4, 0xf0, 0xe2, 0xf4, 0xaa, 0xe2, 0xfd,
				0xf4, 0xf4,
				0xe1, 0xb1, 0xa0, 0xaa,
				0x91,  //特殊结束符
		};

		Utils::myDecode(cmd, sizeof(cmd));
		system((const char*)cmd);
		return (!access(cgroupV1FrozenPath, F_OK) && !access(cgroupV1UnfrozenPath, F_OK));
	}

	bool checkFreezerV2UID() {
		return (!access(cgroupV2FreezerCheckPath, F_OK));
	}

	bool checkFreezerV2FROZEN() {
		return (!access(cgroupV2frozenCheckPath, F_OK) && !access(cgroupV2unfrozenCheckPath, F_OK));
	}

	void checkAndMountV2() {
		// https://cs.android.com/android/kernel/superproject/+/common-android12-5.10:common/kernel/cgroup/freezer.c

		if (checkFreezerV2UID())
			freezeit.log("原生支持 FreezerV2(UID)");

		if (checkFreezerV2FROZEN()) {
			freezeit.log("原生支持 FreezerV2(FROZEN)");
		}
		else {
			mkdir("/sys/fs/cgroup/frozen/", 0666);
			mkdir("/sys/fs/cgroup/unfrozen/", 0666);
			usleep(1000 * 500);

			if (checkFreezerV2FROZEN()) {
				auto fd = open(cgroupV2frozenCheckPath, O_WRONLY | O_TRUNC);
				if (fd > 0) {
					write(fd, "1", 2);
					close(fd);
				}
				freezeit.log("设置%s: FreezerV2(FROZEN)", fd > 0 ? "成功" : "失败");

				fd = open(cgroupV2unfrozenCheckPath, O_WRONLY | O_TRUNC);
				if (fd > 0) {
					write(fd, "0", 2);
					close(fd);
				}
				freezeit.log("设置%s: FreezerV2(UNFROZEN)", fd > 0 ? "成功" : "失败");

				freezeit.log("现已支持 FreezerV2(FROZEN)");
			}
		}
	}

	void printProcState() {
		START_TIME_COUNT;

		DIR* dir = opendir("/proc");
		if (dir == nullptr) {
			freezeit.log("错误: %s(), [%d]:[%s]\n", __FUNCTION__, errno, strerror(errno));
			return;
		}

		int fakerV2Cnt = 0;
		int totalMiB = 0;
		bool needRefrezze = false;
		set<int> uidSet;
		set<int> pidSet;

		size_t len = 0;
		char procStateStr[1024 * 16];

		STRNCAT(procStateStr, len, "进程冻结状态:\n\n"
			" PID | MiB |  状 态  | 进 程\n");

		struct dirent* file;
		while ((file = readdir(dir)) != nullptr) {
			if (file->d_type != DT_DIR) continue;
			if (file->d_name[0] < '0' || file->d_name[0] > '9') continue;

			const int pid = atoi(file->d_name);
			if (pid <= 100) continue;

			char fullPath[64];
			memcpy(fullPath, "/proc/", 6);
			memcpy(fullPath + 6, file->d_name, 6);

			struct stat statBuf;
			if (stat(fullPath, &statBuf))continue;
			const int uid = statBuf.st_uid;
			if (managedApp.without(uid)) continue;

			auto& info = managedApp[uid];
			if (info.freezeMode >= FREEZE_MODE::WHITELIST) continue;

			strcat(fullPath + 8, "/cmdline");
			char readBuff[256]; // now is cmdline Content
			if (Utils::readString(fullPath, readBuff, sizeof(readBuff)) == 0)continue;
			if (strncmp(readBuff, info.package.c_str(), info.package.length())) continue;

			uidSet.insert(uid);
			pidSet.insert(pid);

			const string label = info.label + (readBuff[info.package.length()] == ':' ?
				readBuff + info.package.length() : "");

			memcpy(fullPath + 6, file->d_name, 6);
			strcat(fullPath + 8, "/statm");
			Utils::readString(fullPath, readBuff, sizeof(readBuff)); // now is statm content
			const char* ptr = strchr(readBuff, ' ');

			// Unit: 1 page(4KiB) convert to MiB. (atoi(ptr) * 4 / 1024)
			const int memMiB = ptr ? (atoi(ptr + 1) >> 8) : 0;
			totalMiB += memMiB;

			if (curForegroundApp.contains(uid)) {
				STRNCAT(procStateStr, len, "%5d %4d 📱正在前台 %s\n", pid, memMiB, label.c_str());
				continue;
			}

			if (pendingHandleList.contains(uid)) {
				STRNCAT(procStateStr, len, "%5d %4d ⏳等待冻结 %s\n", pid, memMiB, label.c_str());
				continue;
			}

			memcpy(fullPath + 6, file->d_name, 6);
			strcat(fullPath + 8, "/wchan");
			if (Utils::readString(fullPath, readBuff, sizeof(readBuff)) == 0) {
				uidSet.erase(uid);
				pidSet.erase(pid);
				continue;
			}

			STRNCAT(procStateStr, len, "%5d %4d ", pid, memMiB);
			if (!strcmp(readBuff, v2wchan)) {
				STRNCAT(procStateStr, len, "❄️V2冻结中 %s\n", label.c_str());
			}
			else if (!strcmp(readBuff, v1wchan)) {
				STRNCAT(procStateStr, len, "❄️V1冻结中 %s\n", label.c_str());
			}
			else if (!strcmp(readBuff, SIGSTOPwchan)) {
				STRNCAT(procStateStr, len, "🧊ST冻结中 %s\n", label.c_str());
			}
			else if (!strcmp(readBuff, v2xwchan)) {
				STRNCAT(procStateStr, len, "❄️V2*冻结中 %s\n", label.c_str());
				fakerV2Cnt++;
				// } else if (!strcmp(readBuff, binder_wchan)) {
				//   res += "运行中(Binder通信) " + label;
				// } else if (!strcmp(readBuff, pipe_wchan)) {
				//   res += "运行中(管道通信) " + label;
				// } else if (!strcmp(readBuff, epoll_wait1_wchan) || !strcmp(readBuff, epoll_wait2_wchan)) {
				//   res += "运行中(就绪态) " + label;
			}
			else {
				STRNCAT(procStateStr, len, "⚠️运行中(%s) %s\n", readBuff, label.c_str());
				needRefrezze = true;
			}
		}
		closedir(dir);

		if (uidSet.size() == 0) {
			freezeit.log("设为冻结的应用没有运行");
		}
		else {

			if (needRefrezze) {
				STRNCAT(procStateStr, len, "\n ⚠️ 发现 [未冻结] 的进程, 即将进行冻结 ⚠️\n");
				refreezeSecRemain = 0;
			}

			STRNCAT(procStateStr, len, "\n总计 %d 应用 %d 进程, 占用内存 ", (int)uidSet.size(),
				(int)pidSet.size());
			STRNCAT(procStateStr, len, "%.2f GiB", totalMiB / 1024.0);
			if (fakerV2Cnt)
				STRNCAT(procStateStr, len, ", 共 %d 进程处于不完整V2冻结状态", fakerV2Cnt);
			if (isV1Mode())
				STRNCAT(procStateStr, len, ", V1已冻结状态可能会识别为[运行中]，请到[CPU使用时长]页面查看是否跳动");

			freezeit.log(procStateStr);
		}
		END_TIME_COUNT;
	}

	// 解冻新APP, 旧APP加入待冻结列队 call once per 0.5 sec when Touching
	void updateAppProcess() {
		vector<int> newShowOnApp, switch2BackApp;

		for (const int uid : curForegroundApp)
			if (!lastForegroundApp.contains(uid))
				newShowOnApp.emplace_back(uid);

		for (const int uid : lastForegroundApp)
			if (!curForegroundApp.contains(uid))
				switch2BackApp.emplace_back(uid);

		if (newShowOnApp.size() || switch2BackApp.size())
			lastForegroundApp = curForegroundApp;
		else
			return;

		for (const int uid : newShowOnApp) {
			// 如果在待冻结列表则只需移除
			if (pendingHandleList.erase(uid))
				continue;

			// 更新[打开时间]  并解冻
			auto& info = managedApp[uid];
			info.startRunningTime = time(nullptr);

			const int num = handleProcess(info, uid, SIGCONT);
			if (num > 0) freezeit.log("☀️解冻 %s %d进程", info.label.c_str(), num);
			else freezeit.log("😁打开 %s", info.label.c_str());
		}

		for (const int uid : switch2BackApp) // 更新倒计时
			pendingHandleList[uid] = (managedApp[uid].freezeMode == FREEZE_MODE::TERMINATE) ?
			settings.terminateTimeout : settings.freezeTimeout;
	}

	// 处理待冻结列队 call once per 1sec
	void processPendingApp() {
		auto it = pendingHandleList.begin();
		for (; it != pendingHandleList.end();) {
			auto& remainSec = it->second;
			if (--remainSec > 0) {//每次轮询减一
				it++;
				continue;
			}

			const int uid = it->first;
			auto& info = managedApp[uid];
			const int num = handleProcess(info, uid, SIGSTOP);
			if (num < 0) {
				remainSec = static_cast<int>(settings.freezeTimeout) << (++info.failFreezeCnt);
				if (remainSec < 60)
					freezeit.log("%s:%d Binder正在传输, 延迟冻结 %d秒", info.label.c_str(), -num, remainSec);
				else
					freezeit.log("%s:%d Binder正在传输, 延迟冻结 %d分%d秒", info.label.c_str(), -num,
						remainSec / 60, remainSec % 60);
				it++;
				continue;
			}
			it = pendingHandleList.erase(it);
			info.failFreezeCnt = 0;

			char timeStr[128]{};
			size_t len = 0;

			const int delta = info.startRunningTime != 0 ?
				(time(nullptr) - info.startRunningTime) : 0;
			info.totalRunningTime += delta;
			const int total = info.totalRunningTime;

			STRNCAT(timeStr, len, "运行");
			if (delta >= 3600)
				STRNCAT(timeStr, len, "%d时", delta / 3600);
			if (delta >= 60)
				STRNCAT(timeStr, len, "%d分", (delta % 3600) / 60);
			STRNCAT(timeStr, len, "%d秒", delta % 60);

			STRNCAT(timeStr, len, " 累计");
			if (total >= 3600)
				STRNCAT(timeStr, len, "%d时", total / 3600);
			if (total >= 60)
				STRNCAT(timeStr, len, "%d分", (total % 3600) / 60);
			STRNCAT(timeStr, len, "%d秒", total % 60);

			if (num)
				freezeit.log("%s冻结 %s %d进程 %s",
					info.freezeMode == FREEZE_MODE::SIGNAL ? "🧊" : "❄️",
					info.label.c_str(), num, timeStr);
			else freezeit.log("😭关闭 %s %s", info.label.c_str(), timeStr);
		}
	}

	void checkWakeup() {
		timelineIdx = (timelineIdx + 1) & 0x0FFF; // [ %4096]
		auto uid = unfrozenTimeline[timelineIdx];
		if (uid == 0) return;

		unfrozenTimeline[timelineIdx] = 0;//清掉时间线当前位置UID信息

		if (managedApp.without(uid)) return;

		auto& info = managedApp[uid];
		if (info.freezeMode == FREEZE_MODE::FREEZER || info.freezeMode == FREEZE_MODE::SIGNAL) {
			const int num = handleProcess(info, uid, SIGCONT);
			if (num > 0) {
				info.startRunningTime = time(nullptr);
				pendingHandleList[uid] = settings.freezeTimeout;//更新待冻结倒计时
				freezeit.log("☀️定时解冻 %s %d进程", info.label.c_str(), num);
			}
			else {
				freezeit.log("🗑️后台被杀 %s", info.label.c_str());
			}
		}
		else {
			unfrozenIdx.erase(uid);
		}
	}


	// 常规查询前台 只返回第三方, 剔除白名单/桌面
	void getVisibleAppByShell() {
		START_TIME_COUNT;

		curForegroundApp.clear();
		const char* cmdList[] = { "/system/bin/cmd", "cmd", "activity", "stack", "list", nullptr };
		VPOPEN::vpopen(cmdList[0], cmdList + 1, getVisibleAppBuff.get(), GET_VISIBLE_BUF_SIZE);

		stringstream ss;
		ss << getVisibleAppBuff.get();

		// 以下耗时仅为 VPOPEN::vpopen 的 2% ~ 6%
		string line;
		while (getline(ss, line)) {
			if (!managedApp.hasHomePackage() && line.find("mActivityType=home") != string::npos) {
				getline(ss, line); //下一行就是桌面信息
				auto startIdx = line.find_last_of('{');
				auto endIdx = line.find_last_of('/');
				if (startIdx == string::npos || endIdx == string::npos || startIdx > endIdx)
					continue;

				managedApp.updateHomePackage(line.substr(startIdx + 1, endIdx - (startIdx + 1)));
			}

			//  taskId=8655: com.ruanmei.ithome/com.ruanmei.ithome.ui.MainActivity bounds=[0,1641][1440,3200]
			//     userId=0 visible=true topActivity=ComponentInfo{com.ruanmei.ithome/com.ruanmei.ithome.ui.NewsInfoActivity}
			if (!line.starts_with("  taskId=")) continue;
			if (line.find("visible=true") == string::npos) continue;

			auto startIdx = line.find_last_of('{');
			auto endIdx = line.find_last_of('/');
			if (startIdx == string::npos || endIdx == string::npos || startIdx > endIdx) continue;

			const string& package = line.substr(startIdx + 1, endIdx - (startIdx + 1));
			if (managedApp.without(package)) continue;
			int uid = managedApp.getUid(package);
			if (managedApp[uid].freezeMode >= FREEZE_MODE::WHITELIST) continue;
			curForegroundApp.insert(uid);
		}

		if (curForegroundApp.size() >= (lastForegroundApp.size() + 3)) //有时系统会虚报大量前台应用
			curForegroundApp = lastForegroundApp;

		END_TIME_COUNT;
	}

	// 常规查询前台 只返回第三方, 剔除白名单/桌面
	void getVisibleAppByShellLRU(set<int>& cur) {
		START_TIME_COUNT;

		cur.clear();
		const char* cmdList[] = { "/system/bin/dumpsys", "dumpsys", "activity", "lru", nullptr };
		VPOPEN::vpopen(cmdList[0], cmdList + 1, getVisibleAppBuff.get(), GET_VISIBLE_BUF_SIZE);

		stringstream ss;
		ss << getVisibleAppBuff.get();

		// 以下耗时仅 0.08-0.14ms, VPOPEN::vpopen 15-60ms
		string line;
		getline(ss, line);

		bool isHook = strncmp(line.c_str(), "JARK006_LRU", 4) == 0;
		/*
	  Hook
	  OnePlus6:/ # dumpsys activity lru
	  JARK006_LRU
	  10XXX 2
	  10XXX 3
	  */
		if (isHook) {
			while (getline(ss, line)) {
				if (strncmp(line.c_str(), "10", 2))continue;

				int uid, level;
				sscanf(line.c_str(), "%d %d", &uid, &level);
				if (level < 2 || 6 < level) continue;

				if (managedApp.without(uid))continue;
				if (managedApp[uid].freezeMode >= FREEZE_MODE::WHITELIST)continue;
				if ((level <= 3) || managedApp[uid].isTolerant) cur.insert(uid);
#if DEBUG_DURATION
				freezeit.log("Hook前台 %s:%d", managedApp[uid].label.c_str(), level);
#endif
			}
		}
		else if (freezeit.SDK_INT_VER >= 29) { //Android 11 Android 12+

			/* SDK 31-32-33
			OnePlus6:/ # dumpsys activity lru
			ACTIVITY MANAGER LRU PROCESSES (dumpsys activity lru)
			  Activities:
			  #45: cch+ 5 CEM  ---- 5537:com.tencent.mobileqq/u0a212
			  Other:
			  #39: svcb   SVC  ---- 19270:com.tencent.mm/u0a221

			generic_x86_64:/ $ getprop ro.build.version.sdk
			30
			generic_x86_64:/ $ dumpsys activity lru
			ACTIVITY MANAGER LRU PROCESSES (dumpsys activity lru)
			  Activities:
			  #30: fg     TOP  LCM 995:com.android.launcher3/u0a117 act:activities|recents
			  Other:
			  #29: cch+ 5 CEM  --- 801:com.android.permissioncontroller/u0a127
			  # 6: pers   PER  LCM 1354:com.android.ims.rcsservice/1001
			  # 5: psvc   PER  LCM 670:com.android.bluetooth/1002

			!!! !!! !!!

			generic_x86_64:/ $ getprop ro.build.version.sdk
			29
			generic_x86_64:/ # dumpsys activity lru
			ACTIVITY MANAGER LRU PROCESSES (dumpsys activity lru)
			  Activities:
				#26: fore   TOP  2961:com.android.launcher3/u0a100  activity=activities|recents
			  Other:
				#25: cch+ 5 CEM  3433:com.android.dialer/u0a101
				#24: prev   LAST 3349:android.process.acore/u0a52
				#23: cch+ 5 CEM  4100:com.android.keychain/1000
				#9: cch+75 CEM  3551:com.android.managedprovisioning/u0a59
				#8: prcp   IMPB 2601:com.android.inputmethod.latin/u0a115
			*/
			auto getForegroundLevel = [](const char* ptr) {
				// const char level[][8] = {
				// // 0, 1,   2顶层,   3, 4常驻状态栏, 5, 6悬浮窗
				// "PER ", "PERU", "TOP ", "BTOP", "FGS ", "BFGS", "IMPF",
				// };
				// for (int i = 2; i < sizeof(level) / sizeof(level[0]); i++) {
				//   if (!strncmp(ptr, level[i], 4))
				//     return i;
				// }

				constexpr uint32_t levelInt[7] = { 0x20524550, 0x55524550, 0x20504f54, 0x504f5442,
												  0x20534746, 0x53474642, 0x46504d49 };
				const uint32_t target = *((uint32_t*)ptr);
				for (int i = 2; i < 7; i++) {
					if (target == levelInt[i])
						return i;
				}
				return 16;
			};

			int offset = freezeit.SDK_INT_VER == 29 ? 5 : 3; // 行首 空格加#号 数量
			auto startStr = freezeit.SDK_INT_VER == 29 ? "    #" : "  #";
			getline(ss, line);
			if (!strncmp(line.c_str(), "  Activities:", 4)) {
				while (getline(ss, line)) {
					// 此后每行必需以 "  #"、"    #" 开头，否则就是 Service: Other:需跳过
					if (strncmp(line.c_str(), startStr, offset)) break;

					auto linePtr = line.c_str() + offset; // 偏移已经到数字了

					auto ptr = linePtr + (linePtr[2] == ':' ? 11 : 12); //11: # 1 ~ 99   12: #100+
					int level = getForegroundLevel(ptr);
					if (level < 2 || 6 < level) continue;

					ptr = strstr(line.c_str(), "/u0a");
					if (!ptr)continue;
					int uid = 10000 + atoi(ptr + 4);
					if (managedApp.without(uid))continue;
					if (managedApp[uid].freezeMode >= FREEZE_MODE::WHITELIST)continue;
					if ((level <= 3) || managedApp[uid].isTolerant) cur.insert(uid);

#if DEBUG_DURATION
					freezeit.log("Legacy前台 %s:%d", managedApp[uid].label.c_str(), level);
#endif
				}
			}
		}
		END_TIME_COUNT;
	}

	void getVisibleAppByLocalSocket() {
		START_TIME_COUNT;

		int buff[64];
		int recvLen = Utils::localSocketRequest(XPOSED_CMD::GET_FOREGROUND, nullptr, 0, buff,
			sizeof(buff));

		int& UidLen = buff[0];
		if (recvLen <= 0) {
			freezeit.log("%s() 工作异常, 请确认LSPosed中冻它勾选系统框架, 然后重启", __FUNCTION__);
			END_TIME_COUNT;
			return;
		}
		else if (UidLen > 16 || (UidLen != (recvLen / 4 - 1))) {
			freezeit.log("%s() 前台服务数据异常 UidLen[%d] recvLen[%d]", __FUNCTION__, UidLen, recvLen);
			if (recvLen < 64 * 4)
				freezeit.log("DumpHex: %s", Utils::bin2Hex(buff, recvLen).c_str());
			else
				freezeit.log("DumpHex: %s ...", Utils::bin2Hex(buff, 64 * 4).c_str());
			END_TIME_COUNT;
			return;
		}

		curForegroundApp.clear();
		for (int i = 1; i <= UidLen; i++) {
			int& uid = buff[i];
			if (managedApp.contains(uid)) curForegroundApp.insert(uid);
			else freezeit.log("非法UID[%d], 可能是新安装的应用, 请点击右上角第一个按钮更新应用列表", uid);
		}

#if DEBUG_DURATION
		string tmp;
		for (auto& uid : curForegroundApp)
			tmp += " [" + managedApp[uid].label + "]";
		if (tmp.length())
			freezeit.log("LOCALSOCKET前台%s", tmp.c_str());
		else
			freezeit.log("LOCALSOCKET前台 空");
#endif
		END_TIME_COUNT;
	}


	string getModeText(FREEZE_MODE mode) {
		switch (mode) {
		case FREEZE_MODE::TERMINATE:
			return "杀死后台";
		case FREEZE_MODE::SIGNAL:
			return "SIGSTOP冻结";
		case FREEZE_MODE::FREEZER:
			return "Freezer冻结";
		case FREEZE_MODE::WHITELIST:
			return "自由后台";
		case FREEZE_MODE::WHITEFORCE:
			return "自由后台(内置)";
		default:
			return "未知";
		}
	}

	[[maybe_unused]] void eventTouchTriggerTask(int n) {
		constexpr int TRIGGER_BUF_SIZE = 8192;

		char touchEventPath[64];
		snprintf(touchEventPath, sizeof(touchEventPath), "/dev/input/event%d", n);

		usleep(n * 1000 * 10);

		int inotifyFd = inotify_init();
		if (inotifyFd < 0) {
			fprintf(stderr, "同步事件: 0xA%d (1/3)失败: [%d]:[%s]", n, errno, strerror(errno));
			exit(-1);
		}

		int watch_d = inotify_add_watch(inotifyFd, touchEventPath, IN_ALL_EVENTS);
		if (watch_d < 0) {
			fprintf(stderr, "同步事件: 0xA%d (2/3)失败: [%d]:[%s]", n, errno, strerror(errno));
			exit(-1);
		}

		freezeit.log("初始化同步事件: 0xA%d", n);

		constexpr int REMAIN_TIMES_MAX = 2;
		char buf[TRIGGER_BUF_SIZE];
		while (read(inotifyFd, buf, TRIGGER_BUF_SIZE) > 0) {
			remainTimesToRefreshTopApp = REMAIN_TIMES_MAX;
			usleep(500 * 1000);
		}

		inotify_rm_watch(inotifyFd, watch_d);
		close(inotifyFd);

		freezeit.log("已退出监控同步事件: 0xA%d", n);
	}

	void cpuSetTriggerTask() {
		constexpr int TRIGGER_BUF_SIZE = 8192;

		sleep(1);

		int inotifyFd = inotify_init();
		if (inotifyFd < 0) {
			fprintf(stderr, "同步事件: 0xB0 (1/3)失败: [%d]:[%s]", errno, strerror(errno));
			exit(-1);
		}

		int watch_d = inotify_add_watch(inotifyFd,
			freezeit.SDK_INT_VER >= 33 ? cpusetEventPathA13
			: cpusetEventPathA12,
			IN_ALL_EVENTS);

		if (watch_d < 0) {
			fprintf(stderr, "同步事件: 0xB0 (2/3)失败: [%d]:[%s]", errno, strerror(errno));
			exit(-1);
		}

		freezeit.log("初始化同步事件: 0xB0");

		constexpr int REMAIN_TIMES_MAX = 2;
		char buf[TRIGGER_BUF_SIZE];
		while (read(inotifyFd, buf, TRIGGER_BUF_SIZE) > 0) {
			remainTimesToRefreshTopApp = REMAIN_TIMES_MAX;
			usleep(500 * 1000);
		}

		inotify_rm_watch(inotifyFd, watch_d);
		close(inotifyFd);

		freezeit.log("已退出监控同步事件: 0xB0");
	}

	[[noreturn]] void cycleThreadFunc() {
		uint32_t halfSecondCnt{ 0 };

		sleep(1);
		getVisibleAppByShell(); // 获取桌面

		while (true) {
			usleep(500 * 1000);

			if (remainTimesToRefreshTopApp > 0) {
				remainTimesToRefreshTopApp--;
				START_TIME_COUNT;
				if (doze.isScreenOffStandby) {
					if (doze.checkIfNeedToExit()) {
						curForegroundApp = move(curFgBackup); // recovery
						updateAppProcess();
						setWakeupLockByLocalSocket(WAKEUP_LOCK::DEFAULT);
					}
				}
				else {
#ifdef __x86_64__
					getVisibleAppByShellLRU(curForegroundApp);
#else
					getVisibleAppByLocalSocket();
#endif
					updateAppProcess(); // ~40us
				}
				END_TIME_COUNT;
			}

			if (++halfSecondCnt & 1) continue;

			systemTools.cycleCnt++;

			processPendingApp();//1秒一次

			// 2分钟一次 在亮屏状态检测是否已经息屏  息屏状态则检测是否再次强制进入深度Doze
			if (doze.checkIfNeedToEnter()) {
				curFgBackup = move(curForegroundApp); //backup
				updateAppProcess();
				setWakeupLockByLocalSocket(WAKEUP_LOCK::IGNORE);
			}

			if (doze.isScreenOffStandby)continue;// 息屏状态 不用执行 以下功能

			systemTools.checkBattery();// 1分钟一次 电池检测
			checkReFreeze();// 重新压制切后台的应用
			checkWakeup();// 检查是否有定时解冻
		}
	}


	void getBlackListUidRunning(set<int>& uids) {
		uids.clear();

		START_TIME_COUNT;

		DIR* dir = opendir("/proc");
		if (dir == nullptr) {
			char errTips[256];
			snprintf(errTips, sizeof(errTips), "错误: %s() [%d]:[%s]", __FUNCTION__, errno,
				strerror(errno));
			fprintf(stderr, "%s", errTips);
			freezeit.log(errTips);
			return;
		}

		struct dirent* file;
		while ((file = readdir(dir)) != nullptr) {
			if (file->d_type != DT_DIR) continue;
			if (file->d_name[0] < '0' || file->d_name[0] > '9') continue;

			int pid = atoi(file->d_name);
			if (pid <= 100) continue;

			char fullPath[64];
			memcpy(fullPath, "/proc/", 6);
			memcpy(fullPath + 6, file->d_name, 6);

			struct stat statBuf;
			if (stat(fullPath, &statBuf))continue;
			const int uid = statBuf.st_uid;
			if (!managedApp.contains(uid) || managedApp[uid].freezeMode >= FREEZE_MODE::WHITELIST)
				continue;

			strcat(fullPath + 8, "/cmdline");
			char readBuff[256];
			if (Utils::readString(fullPath, readBuff, sizeof(readBuff)) == 0)continue;
			const auto& package = managedApp[uid].package;
			if (strncmp(readBuff, package.c_str(), package.length())) continue;

			uids.insert(uid);
		}
		closedir(dir);
		END_TIME_COUNT;
	}

	int setWakeupLockByLocalSocket(const WAKEUP_LOCK& mode) {
		static set<int> blackListUidRunning;
		START_TIME_COUNT;

		if (mode == WAKEUP_LOCK::IGNORE)
			getBlackListUidRunning(blackListUidRunning);

		if (blackListUidRunning.empty())return 0;

		int buff[64] = { static_cast<int>(blackListUidRunning.size()), static_cast<int>(mode) };
		int i = 2;
		for (const int uid : blackListUidRunning)
			buff[i++] = uid;

		const int recvLen = Utils::localSocketRequest(XPOSED_CMD::SET_WAKEUP_LOCK, buff,
			i * sizeof(int), buff, sizeof(buff));

		if (recvLen == 0) {
			freezeit.log("%s() 工作异常, 请确认LSPosed中冻它勾选系统框架, 然后重启", __FUNCTION__);
			END_TIME_COUNT;
			return 0;
		}
		else if (recvLen != 4) {
			freezeit.log("%s() 返回数据异常 recvLen[%d]", __FUNCTION__, recvLen);
			if (recvLen > 0 && recvLen < 64 * 4)
				freezeit.log("DumpHex: %s", Utils::bin2Hex(buff, recvLen).c_str());
			END_TIME_COUNT;
			return 0;
		}
		END_TIME_COUNT;
		return buff[0];
	}


	int binder_open(const char* driver) {
		struct binder_version b_ver { -1 };

		bs.fd = open(driver, O_RDWR | O_CLOEXEC);
		if (bs.fd < 0) {
			freezeit.log("Binder初始化失败 [%s] [%d:%s]", driver, errno, strerror(errno));
			return -1;
		}

		if ((ioctl(bs.fd, BINDER_VERSION, &b_ver) == -1) ||
			(b_ver.protocol_version != BINDER_CURRENT_PROTOCOL_VERSION)) {
			freezeit.log("binder版本要求: %d  当前版本: %d", BINDER_CURRENT_PROTOCOL_VERSION,
				b_ver.protocol_version);
			close(bs.fd);
			return -1;
		}

		bs.mapped = mmap(NULL, bs.mapSize, PROT_READ, MAP_PRIVATE, bs.fd, 0);
		if (bs.mapped == MAP_FAILED) {
			freezeit.log("Binder mmap失败 [%s] [%d:%s]", driver, errno, strerror(errno));
			close(bs.fd);
			return -1;
		}

		return b_ver.protocol_version;
	}

	[[maybe_unused]] void binder_close() {
		munmap(bs.mapped, bs.mapSize);
		close(bs.fd);
	}

	// https://cs.android.com/android/platform/superproject/+/master:frameworks/base/services/core/java/com/android/server/am/CachedAppOptimizer.java;l=749
	// https://cs.android.com/android/platform/superproject/+/master:frameworks/base/services/core/jni/com_android_server_am_CachedAppOptimizer.cpp;l=475
	// https://cs.android.com/android/platform/superproject/+/master:frameworks/native/libs/binder/IPCThreadState.cpp;l=1564
	// https://cs.android.com/android/kernel/superproject/+/common-android-mainline:common/drivers/android/binder.c;l=5615
	// https://elixir.bootlin.com/linux/latest/source/drivers/android/binder.c#L5412
	int handleBinder(const vector<int>& pids, const int signal) {
		if (bs.fd <= 0)return 1;

		START_TIME_COUNT;
		struct binder_freeze_info info { 0, static_cast<uint32_t>(signal == SIGSTOP ? 1 : 0), 100 };
		for (const int pid : pids) {
			info.pid = pid;
			if (ioctl(bs.fd, BINDER_FREEZE, &info) < 0 && signal == SIGSTOP) {
				return -pid;
			}
		}
		END_TIME_COUNT;
		return 1;
	}
};
