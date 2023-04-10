#pragma once

#include "utils.hpp"
#include "freezeit.hpp"
#include "settings.hpp"
#include "vpopen.hpp"


class ManagedApp {
private:
	string cfgPath;
	string labelPath;

	Freezeit& freezeit;
	Settings& settings;

	static const size_t PACKAGE_LIST_BUF_SIZE = 256 * 1024;
	unique_ptr<char[]> packageListBuff;

	string homePackage;
	map<int, appInfoStruct> infoMap;
	map<string, int> uidIndex;
	map<int, cfgStruct> cfgTemp;

	const unordered_set<string> whiteListForce{
			"com.xiaomi.mibrain.speech",            // 系统语音引擎
			"com.xiaomi.scanner",                   // 小爱视觉
			"com.xiaomi.xmsf",                      // Push
			"com.xiaomi.xmsfkeeper",                // Push
			"com.xiaomi.misettings",                // 设置
			"com.xiaomi.barrage",                   // 弹幕通知
			"com xiaomi.aireco",                    // 小爱建议
			"com.xiaomi.account",                   // 小米账号
			"com.mfashiongallery.emag",             // 小米画报
			"com.huawei.hwid",                      // HMS core服务

			"cn.litiaotiao.app",                    // 李跳跳
			"com.litiaotiao.app",                   // 李跳跳
			"hello.litiaotiao.app",                 // 李跳跳
			"com.topjohnwu.magisk",                 // Magisk
			"io.github.vvb2060.magisk",             // Magisk Alpha
			"io.github.huskydg.magisk",             // Magisk Delta
			"io.github.jark006.freezeit",           // 冻它
			"io.github.jark006.weather",            // 小天气
			"com.jark006.weather",                  // 小天气
			"org.lsposed.manager",                  // LSPosed
			"com.github.tianma8023.xposed.smscode", // XposedSmsCode
			"com.merxury.blocker",                  // Blocker
			"com.wpengapp.lightstart",              // 轻启动
			"name.monwf.customiuizer",              // 米客 原版
			"name.mikanoshi.customiuizer",          // 米客

			"org.meowcat.xposed.mipush",            // 小米推送框架增强
			"top.trumeet.mipush",                   // 小米推送服务
			"one.yufz.hmspush",                     // HMSPush服务

			"app.lawnchair",                        // Lawnchair
			"com.microsoft.launcher",               // 微软桌面
			"com.teslacoilsw.launcher",             // Nova Launcher
			"com.hola.launcher",                    // Hola桌面
			"com.transsion.XOSLauncher",            // XOS桌面
			"com.mi.android.globallauncher",        // POCO桌面
			"com.gau.go.launcherex",                // GO桌面
			"bitpit.launcher",                      // Niagara Launcher
			"com.google.android.apps.nexuslauncher",// pixel 桌面
			"com.oppo.launcher",

			"me.weishu.kernelsu",                   // KernelSU
			"top.canyie.dreamland.manager",         // Dreamland

			"com.miui.home",
			"com.miui.carlink",
			"com.miui.packageinstaller",            // 安装包管理
			"com.coloros.packageinstaller",         // 安装包管理
			"com.oplus.packageinstaller",           // 安装包管理
			"com.iqoo.packageinstaller",            // 安装包管理
			"com.vivo.packageinstaller",            // 安装包管理
			"com.google.android.packageinstaller",  // 软件包安装程序


			"com.baidu.input",                            //百度输入法
			"com.baidu.input_huawei",                     //百度输入法华为版
			"com.baidu.input_mi",                         //百度输入法小米版
			"com.baidu.input_oppo",                       //百度输入法OPPO版
			"com.baidu.input_vivo",                       //百度输入法VIVO版
			"com.baidu.input_yijia",                      //百度输入法一加版

			"com.sohu.inputmethod.sogou",                 //搜狗输入法
			"com.sohu.inputmethod.sogou.xiaomi",          //搜狗输入法小米版
			"com.sohu.inputmethod.sogou.meizu",           //搜狗输入法魅族版
			"com.sohu.inputmethod.sogou.nubia",           //搜狗输入法nubia版
			"com.sohu.inputmethod.sogou.chuizi",          //搜狗输入法chuizi版
			"com.sohu.inputmethod.sogou.moto",            //搜狗输入法moto版
			"com.sohu.inputmethod.sogou.zte",             //搜狗输入法中兴版
			"com.sohu.inputmethod.sogou.samsung",         //搜狗输入法samsung版
			"com.sohu.input_yijia",                       //搜狗输入法一加版

			"com.iflytek.inputmethod",                    //讯飞输入法
			"com.iflytek.inputmethod.miui",               //讯飞输入法小米版
			"com.iflytek.inputmethod.googleplay",         //讯飞输入法googleplay版
			"com.iflytek.inputmethod.smartisan",          //讯飞输入法smartisan版
			"com.iflytek.inputmethod.oppo",               //讯飞输入法oppo版
			"com.iflytek.inputmethod.oem",                //讯飞输入法oem版
			"com.iflytek.inputmethod.custom",             //讯飞输入法custom版
			"com.iflytek.inputmethod.blackshark",         //讯飞输入法blackshark版
			"com.iflytek.inputmethod.zte",                //讯飞输入法zte版

			"com.tencent.qqpinyin",                       // QQ拼音输入法
			"com.google.android.inputmethod.latin",       //谷歌Gboard输入法
			"com.touchtype.swiftkey",                     //微软swiftkey输入法
			"com.touchtype.swiftkey.beta",                //微软swiftkeyBeta输入法
			"im.weshine.keyboard",                        // KK键盘输入法
			"com.komoxo.octopusime",                      //章鱼输入法
			"com.qujianpan.duoduo",                       //见萌输入法
			"com.lxlm.lhl.softkeyboard",                  //流行输入法
			"com.jinkey.unfoldedime",                     //不折叠输入法
			"com.iflytek.inputmethods.DungkarIME",        //东噶藏文输入法
			"com.oyun.qingcheng",                         //奥云蒙古文输入法
			"com.ziipin.softkeyboard",                    // Badam维语输入法
			"com.kongzue.secretinput",                    // 密码键盘


			"com.google.android.ext.services",
			"com.google.android.ext.shared",

			"com.android.launcher",
			"com.android.launcher2",
			"com.android.launcher3",
			"com.android.launcher4",
			"com.android.apps.tag", // Tags
			"com.android.bips", // 系统打印服务
			"com.android.bluetoothmidiservice", // Bluetooth MIDI Service
			"com.android.cameraextensions", // CameraExtensionsProxy
			"com.android.captiveportallogin", // CaptivePortalLogin
			"com.android.carrierdefaultapp", // 运营商默认应用
			"com.android.certinstaller", // 证书安装程序
			"com.android.companiondevicemanager", // 配套设备管理器
			"com.android.connectivity.resources", // 系统网络连接资源
			"com.android.contacts", // 通讯录与拨号
			"com.android.deskclock", // 时钟
			"com.android.dreams.basic", // 基本互动屏保
			"com.android.egg", // Android S Easter Egg
			"com.android.emergency", // 急救信息
			"com.android.externalstorage", // 外部存储设备
			"com.android.hotspot2.osulogin", // OsuLogin
			"com.android.htmlviewer", // HTML 查看器
			"com.android.incallui", // 电话
			"com.android.internal.display.cutout.emulation.corner", // 边角刘海屏
			"com.android.internal.display.cutout.emulation.double", // 双刘海屏
			"com.android.internal.display.cutout.emulation.hole", // 打孔屏
			"com.android.internal.display.cutout.emulation.tall", // 长型刘海屏
			"com.android.internal.display.cutout.emulation.waterfall", // 瀑布刘海屏
			"com.android.internal.systemui.navbar.gestural", // Gestural Navigation Bar
			"com.android.internal.systemui.navbar.gestural_extra_wide_back", // Gestural Navigation Bar
			"com.android.internal.systemui.navbar.gestural_narrow_back", // Gestural Navigation Bar
			"com.android.internal.systemui.navbar.gestural_wide_back", // Gestural Navigation Bar
			"com.android.internal.systemui.navbar.threebutton", // 3 Button Navigation Bar
			"com.android.managedprovisioning", // 工作设置
			"com.android.mms", // 短信
			"com.android.modulemetadata", // Module Metadata
			"com.android.mtp", // MTP 主机
			"com.android.musicfx", // MusicFX
			"com.android.networkstack.inprocess.overlay", // NetworkStackInProcessResOverlay
			"com.android.networkstack.overlay", // NetworkStackOverlay
			"com.android.networkstack.tethering.inprocess.overlay", // TetheringResOverlay
			"com.android.networkstack.tethering.overlay", // TetheringResOverlay
			"com.android.packageinstaller", // 软件包安装程序
			"com.android.pacprocessor", // PacProcessor
			"com.android.permissioncontroller", // 权限控制器
			"com.android.printspooler", // 打印处理服务
			"com.android.providers.calendar", // 日历存储
			"com.android.providers.contacts", // 联系人存储
			"com.android.providers.downloads.ui", // 下载管理
			"com.android.providers.media.module", // 媒体存储设备
			"com.android.proxyhandler", // ProxyHandler
			"com.android.server.telecom.overlay.miui", // 通话管理
			"com.android.settings.intelligence", // 设置建议
			"com.android.simappdialog", // Sim App Dialog
			"com.android.soundrecorder", // 录音机
			"com.android.statementservice", // 意图过滤器验证服务
			"com.android.storagemanager", // 存储空间管理器
			"com.android.theme.font.notoserifsource", // Noto Serif / Source Sans Pro
			"com.android.traceur", // 系统跟踪
			"com.android.uwb.resources", // System UWB Resources
			"com.android.vpndialogs", // VpnDialogs
			"com.android.wallpaper.livepicker", // Live Wallpaper Picker
			"com.android.wifi.resources", // 系统 WLAN 资源
			"com.debug.loggerui", // DebugLoggerUI
			"com.fingerprints.sensortesttool", // Sensor Test Tool
			"com.lbe.security.miui", // 权限管理服务
			"com.mediatek.callrecorder", // 通话录音机
			"com.mediatek.duraspeed", // 快霸
			"com.mediatek.engineermode", // EngineerMode
			"com.mediatek.lbs.em2.ui", // LocationEM2
			"com.mediatek.location.mtkgeofence", // Mtk Geofence
			"com.mediatek.mdmconfig", // MDMConfig
			"com.mediatek.mdmlsample", // MDMLSample
			"com.mediatek.miravision.ui", // MiraVision
			"com.mediatek.op01.telecom", // OP01Telecom
			"com.mediatek.op09clib.phone.plugin", // OP09ClibTeleService
			"com.mediatek.op09clib.telecom", // OP09ClibTelecom
			"com.mediatek.ygps", // YGPS
			"com.miui.accessibility", // 小米无障碍
			"com.miui.core", // MIUI SDK
			"com.miui.privacycomputing", // MIUI Privacy Components
			"com.miui.securityadd", // 系统服务组件
			"com.miui.securityinputmethod", // 小米安全键盘
			"com.miui.system", // com.miui.internal.app.SystemApplication
			"com.miui.vpnsdkmanager", // MiuiVpnSdkManager
			"com.tencent.soter.soterserver", // SoterService
			"com.unionpay.tsmservice.mi", // 银联可信服务安全组件小米版本


			"android.ext.services", // Android Services Library
			"android.ext.shared", // Android Shared Library
			"com.android.adservices.api", // Android AdServices
			"com.android.bookmarkprovider", // Bookmark Provider
			"com.android.cellbroadcastreceiver.module", // 无线紧急警报
			"com.android.dialer", // 电话
			"com.android.dreams.phototable", // 照片屏幕保护程序
			"com.android.inputmethod.latin", // Android 键盘 (AOSP)
			"com.android.intentresolver", // IntentResolver
			"com.android.internal.display.cutout.emulation.noCutout", // 隐藏
			"com.android.internal.systemui.navbar.twobutton", // 2 Button Navigation Bar
			"com.android.messaging", // 短信
			"com.android.onetimeinitializer", // One Time Init
			"com.android.printservice.recommendation", // Print Service Recommendation Service
			"com.android.safetycenter.resources", // 安全中心资源
			"com.android.soundpicker", // 声音
			"com.android.systemui", // 系统界面
			"com.android.wallpaper", // 壁纸和样式
			"com.qualcomm.qti.cne", // CneApp
			"com.qualcomm.qti.poweroffalarm", // 关机闹钟
			"com.qualcomm.wfd.service", // Wfd Service
			"org.lineageos.aperture", // 相机
			"org.lineageos.audiofx", // AudioFX
			"org.lineageos.backgrounds", // 壁纸
			"org.lineageos.customization", // Lineage Themes
			"org.lineageos.eleven", // 音乐
			"org.lineageos.etar", // 日历
			"org.lineageos.jelly", // 浏览器
			"org.lineageos.overlay.customization.blacktheme", // Black theme
			"org.lineageos.overlay.font.lato", // Lato
			"org.lineageos.overlay.font.rubik", // Rubik
			"org.lineageos.profiles", // 情景模式信任提供器
			"org.lineageos.recorder", // 录音机
			"org.lineageos.updater", // 系统更新
			"org.protonaosp.deviceconfig", // Simple Device Configuration

			"android.aosp.overlay",
			"android.miui.home.launcher.res",
			"android.miui.overlay",
			"com.android.carrierconfig",
			"com.android.carrierconfig.overlay.miui",
			"com.android.incallui.overlay",
			"com.android.managedprovisioning.overlay",
			"com.android.ondevicepersonalization.services",
			"com.android.overlay.cngmstelecomm",
			"com.android.overlay.gmscontactprovider",
			"com.android.overlay.gmssettingprovider",
			"com.android.overlay.gmssettings",
			"com.android.overlay.gmstelecomm",
			"com.android.overlay.gmstelephony",
			"com.android.overlay.systemui",
			"com.android.phone.overlay.miui",
			"com.android.providers.settings.overlay",
			"com.android.sdksandbox",
			"com.android.settings.overlay.miui",
			"com.android.stk.overlay.miui",
			"com.android.systemui.gesture.line.overlay",
			"com.android.systemui.navigation.bar.overlay",
			"com.android.systemui.overlay.miui",
			"com.android.wallpapercropper",
			"com.android.wallpaperpicker",
			"com.android.wifi.dialog",
			"com.android.wifi.resources.overlay",
			"com.android.wifi.resources.xiaomi",
			"com.android.wifi.system.mainline.resources.overlay",
			"com.android.wifi.system.resources.overlay",
			"com.google.android.cellbroadcastreceiver.overlay.miui",
			"com.google.android.cellbroadcastservice.overlay.miui",
			"com.google.android.overlay.gmsconfig",
			"com.google.android.overlay.modules.ext.services",
			"com.google.android.trichromelibrary_511209734",
			"com.google.android.trichromelibrary_541411734",
			"com.mediatek.FrameworkResOverlayExt",
			"com.mediatek.SettingsProviderResOverlay",
			"com.mediatek.batterywarning",
			"com.mediatek.cellbroadcastuiresoverlay",
			"com.mediatek.frameworkresoverlay",
			"com.mediatek.gbaservice",
			"com.mediatek.voiceunlock",
			"com.miui.core.internal.services",
			"com.miui.face.overlay.miui",
			"com.miui.miwallpaper.overlay.customize",
			"com.miui.miwallpaper.wallpaperoverlay.config.overlay",
			"com.miui.rom",
			"com.miui.settings.rro.device.config.overlay",
			"com.miui.settings.rro.device.hide.statusbar.overlay",
			"com.miui.settings.rro.device.type.overlay",
			"com.miui.system.overlay",
			"com.miui.systemui.carriers.overlay",
			"com.miui.systemui.devices.overlay",
			"com.miui.systemui.overlay.devices.android",
			"com.miui.translation.kingsoft",
			"com.miui.translation.xmcloud",
			"com.miui.translationservice",
			"com.miui.voiceassistoverlay",
			"com.miui.wallpaper.overlay.customize",
			"com.xiaomi.bluetooth.rro.device.config.overlay",


			"android.auto_generated_rro_product__",
			"android.auto_generated_rro_vendor__",
			"com.android.backupconfirm",
			"com.android.carrierconfig.auto_generated_rro_vendor__",
			"com.android.cts.ctsshim",
			"com.android.cts.priv.ctsshim",
			"com.android.documentsui.auto_generated_rro_product__",
			"com.android.emergency.auto_generated_rro_product__",
			"com.android.imsserviceentitlement",
			"com.android.imsserviceentitlement.auto_generated_rro_product__",
			"com.android.inputmethod.latin.auto_generated_rro_product__",
			"com.android.launcher3.overlay",
			"com.android.managedprovisioning.auto_generated_rro_product__",
			"com.android.nearby.halfsheet",
			"com.android.phone.auto_generated_rro_vendor__",
			"com.android.providers.settings.auto_generated_rro_product__",
			"com.android.providers.settings.auto_generated_rro_vendor__",
			"com.android.settings.auto_generated_rro_product__",
			"com.android.sharedstoragebackup",
			"com.android.smspush",
			"com.android.storagemanager.auto_generated_rro_product__",
			"com.android.systemui.auto_generated_rro_product__",
			"com.android.systemui.auto_generated_rro_vendor__",
			"com.android.systemui.plugin.globalactions.wallet",
			"com.android.wallpaper.auto_generated_rro_product__",
			"com.android.wifi.resources.oneplus_sdm845",
			"com.qualcomm.timeservice",
			"lineageos.platform.auto_generated_rro_product__",
			"lineageos.platform.auto_generated_rro_vendor__",
			"org.codeaurora.ims",
			"org.lineageos.aperture.auto_generated_rro_vendor__",
			"org.lineageos.lineageparts.auto_generated_rro_product__",
			"org.lineageos.lineagesettings.auto_generated_rro_product__",
			"org.lineageos.lineagesettings.auto_generated_rro_vendor__",
			"org.lineageos.overlay.customization.navbar.nohint",
			"org.lineageos.settings.device.auto_generated_rro_product__",
			"org.lineageos.settings.doze.auto_generated_rro_product__",
			"org.lineageos.settings.doze.auto_generated_rro_vendor__",
			"org.lineageos.setupwizard.auto_generated_rro_product__",
			"org.lineageos.updater.auto_generated_rro_product__",
			"org.protonaosp.deviceconfig.auto_generated_rro_product__",

	};

	const unordered_set<string> whiteListDefault{
		// "com.android.vending",                  // Play 商店
		// "com.google.android.gms",               // GMS 服务
		// "com.google.android.gsf",               // Google 服务框架
		"com.mi.health",                        // 小米运动健康
		"com.tencent.mm.wxa.sce",               // 微信小程序

		"com.onlyone.onlyonestarter",           // 三星系应用
		"com.samsung.accessory.neobeanmgr",     // Galaxy Buds Live Manager
		"com.samsung.app.newtrim",              // 编辑器精简版
		"com.diotek.sec.lookup.dictionary",     // 字典
	};

public:

	const set<FREEZE_MODE> FREEZE_MODE_SET{
			FREEZE_MODE::TERMINATE,
			FREEZE_MODE::FREEZER,
			FREEZE_MODE::SIGNAL,
			FREEZE_MODE::WHITELIST,
			FREEZE_MODE::WHITEFORCE,
	};

	ManagedApp& operator=(ManagedApp&&) = delete;

	ManagedApp(Freezeit& freezeit, Settings& settings) : freezeit(freezeit), settings(settings) {
		cfgPath = freezeit.modulePath + "/appcfg.txt";
		labelPath = freezeit.modulePath + "/applabel.txt";

		packageListBuff = make_unique<char[]>(PACKAGE_LIST_BUF_SIZE);

		updateAppList();
		loadLabelFile();

		loadConfigFile2CfgTemp();
		updateIME2CfgTemp();
		applyCfgTemp();
		update2xposedByLocalSocket();
	}

	auto& getRaw() { return infoMap; }

	auto& operator[](const int& uid) { return infoMap[uid]; }

	auto& operator[](const string& package) { return infoMap[uidIndex[package]]; }

	[[maybe_unused]] size_t erase(const int& uid) { return infoMap.erase(uid); }

	size_t size() { return infoMap.size(); }

	[[maybe_unused]] void clear() { infoMap.clear(); }

	bool contains(const int& uid) { return infoMap.contains(uid); }

	[[maybe_unused]] bool contains(const string& package) { return uidIndex.contains(package); }

	bool without(const int& uid) { return !infoMap.contains(uid); }

	bool without(const string& package) { return !uidIndex.contains(package); }

	auto& getLabel(const int& uid) { return infoMap[uid].label; }

	int getUid(const string& package) { return uidIndex[package]; }

	[[maybe_unused]] int getUidOrDefault(const string& package, const int& defaultValue) {
		auto it = uidIndex.find(package);
		return it != uidIndex.end() ? it->second : defaultValue;
	}

	bool hasHomePackage() { return homePackage.length() > 2; }

	void updateHomePackage(const string& package) {
		homePackage = package;
		const auto& it = uidIndex.find(package);
		if (it == uidIndex.end()) {
			freezeit.log("当前桌面信息异常，建议反馈: [%s]", package.c_str());
			return;
		}

		const int& uid = it->second;
		infoMap[uid].freezeMode = FREEZE_MODE::WHITEFORCE;
	}

	bool readPackagesListA12(map<int, string>& _allAppList, map<int, string>& _thirdAppList) {
		START_TIME_COUNT;

		stringstream ss;
		ss << ifstream("/data/system/packages.list").rdbuf();

		string_view sysEnd("@system");
		string line;
		while (getline(ss, line)) {
			if (line.length() < 10) continue;
			if (line.starts_with("com.google.android.trichromelibrary")) continue;

			int uid;
			char package[256] = {};
			sscanf(line.c_str(), "%s %d", package, &uid);
			if (uid < 10000 || 12000 <= uid) continue;

			const string& packageName{ package };
			_allAppList[uid] = packageName;
			if (!line.ends_with(sysEnd))
				_thirdAppList[uid] = packageName;
		}
		END_TIME_COUNT;
		return _allAppList.size() > 0;
	}

	bool readPackagesListA10_11(map<int, string>& _allAppList) {
		START_TIME_COUNT;

		stringstream ss;
		ss << ifstream("/data/system/packages.list").rdbuf();

		string line;
		while (getline(ss, line)) {
			if (line.length() < 10) continue;
			if (line.starts_with("com.google.android.trichromelibrary")) continue;

			int uid;
			char package[256] = {};
			sscanf(line.c_str(), "%s %d", package, &uid);
			if (uid < 10000 || 12000 <= uid) continue;

			_allAppList[uid] = package;
		}
		END_TIME_COUNT;
		return _allAppList.size() > 0;
	}

	void readCmdPackagesAll(map<int, string>& _allAppList) {
		START_TIME_COUNT;
		stringstream ss;
		string line;

		const char* cmdList[] = { "/system/bin/cmd", "cmd", "package", "list", "packages", "-U",
								 nullptr };
		VPOPEN::vpopen(cmdList[0], cmdList + 1, packageListBuff.get(), PACKAGE_LIST_BUF_SIZE);
		ss << packageListBuff.get();
		while (getline(ss, line)) {
			// package:com.google.android.GoogleCameraGood uid:10364
			if (!Utils::startWith("package:", line.c_str()))continue;
			auto idx = line.find(" uid:");
			if (idx == string::npos)continue;
			int uid = atoi(line.c_str() + idx + 5);

			if (idx < 10 || uid < 10000 || 12000 <= uid) continue;
			_allAppList[uid] = line.substr(8, idx - 8); //package
		}
		END_TIME_COUNT;
	}

	void readCmdPackagesThird(map<int, string>& _thirdAppList) {
		START_TIME_COUNT;
		stringstream ss;
		string line;

		const char* cmdList[] = { "/system/bin/cmd", "cmd", "package", "list", "packages", "-3",
								 "-U", nullptr };
		VPOPEN::vpopen(cmdList[0], cmdList + 1, packageListBuff.get(), PACKAGE_LIST_BUF_SIZE);
		ss << packageListBuff.get();
		while (getline(ss, line)) {
			// package:com.google.android.GoogleCameraGood uid:10364
			if (!Utils::startWith("package:", line.c_str()))continue;
			auto idx = line.find(" uid:");
			if (idx == string::npos)continue;
			int uid = atoi(line.c_str() + idx + 5);

			if (idx < 10 || uid < 10000 || 12000 <= uid) continue;
			_thirdAppList[uid] = line.substr(8, idx - 8); //package
		}
		END_TIME_COUNT;
	}

	// 开机，更新冻结配置，更新应用名称，都会调用
	void updateAppList() {
		START_TIME_COUNT;

		map<int, string> allAppList, thirdAppList;

		if (freezeit.SDK_INT_VER >= 31) {
			if (!readPackagesListA12(allAppList, thirdAppList)) {
				readCmdPackagesAll(allAppList);
				readCmdPackagesThird(thirdAppList);
			}
		}
		else {
			if (!readPackagesListA10_11(allAppList))
				readCmdPackagesAll(allAppList);
			readCmdPackagesThird(thirdAppList);
		}

		if (allAppList.size() == 0) {
			freezeit.log("没有应用或获取失败");
			return;
		}
		else {
			freezeit.log("刷新应用 %lu  系统[%lu] 三方[%lu]",
				allAppList.size(), allAppList.size() - thirdAppList.size(),
				thirdAppList.size());
		}

		uidIndex.clear();
		for (const auto& [uid, package] : allAppList) {
			uidIndex[package] = uid;        // 更新 按包名取UID
			if (infoMap.contains(uid))continue;

			const bool isSYS = !thirdAppList.contains(uid);
			infoMap[uid] = {
					isSYS ? FREEZE_MODE::WHITELIST : FREEZE_MODE::FREEZER, //freezeMode
					true,       // isTolerant
					0,       // failFreezeCnt
					isSYS,   // isSystemApp
					0,       // startRunningTime
					0,       // totalRunningTime
					package, // package
					package, // label
					{}
			};
		}

		// 移除已卸载应用
		for (auto it = infoMap.begin(); it != infoMap.end();) {
			if (allAppList.contains(it->first))it++;
			else it = infoMap.erase(it);
		}
		END_TIME_COUNT;
	}

	void loadConfigFile2CfgTemp() {
		cfgTemp.clear();

		ifstream file(cfgPath);
		if (!file.is_open())
			return;

		string line;
		while (getline(file, line)) {
			if (line.length() <= 4) {
				freezeit.log("A配置错误: [%s]", line.c_str());
				continue;
			}

			auto value = Utils::splitString(line, " ");
			if (value.size() != 3 || value[0].empty()) {
				freezeit.log("B配置错误: [%s]", line.c_str());
				continue;
			}

			int uid;
			if (isdigit(value[0][0])) {
				uid = atoi(value[0].c_str());
			}
			else {
				auto it = uidIndex.find(value[0]);
				if (it == uidIndex.end())continue;
				uid = it->second;
			}
			const FREEZE_MODE freezeMode = static_cast<FREEZE_MODE>(atoi(value[1].c_str()));
			const bool isTolerant = atoi(value[2].c_str()) != 0;

			if (!FREEZE_MODE_SET.contains(freezeMode)) {
				freezeit.log("C配置错误: [%s]", line.c_str());
				continue;
			}

			cfgTemp[uid] = { freezeMode, isTolerant };
		}
		file.close();
	}

	void loadConfig2CfgTemp(map<int, cfgStruct>& newCfg) {
		cfgTemp = std::move(newCfg);
	}

	void updateIME2CfgTemp() {
		const char* cmdList[] = { "/system/bin/ime", "ime", "list", "-s", nullptr };
		char buf[1024 * 4];
		VPOPEN::vpopen(cmdList[0], cmdList + 1, buf, sizeof(buf));

		stringstream ss;
		ss << buf;

		string line;
		while (getline(ss, line)) {

			auto idx = line.find_first_of('/');
			if (idx == string::npos) continue;

			const string& package = line.substr(0, idx);
			if (package.length() < 6) continue;

			auto it = uidIndex.find(package);
			if (it == uidIndex.end()) continue;

			cfgTemp[it->second] = { FREEZE_MODE::WHITEFORCE, 0 };
		}
	}

	bool isSystemApp(const char* ptr) {
		const char* prefix[] = {
				"com.miui.",
				"com.oplus.",
				"com.coloros.",
				"com.heytap.",
				"com.samsung.android.",
				"com.samsung.systemui.",
				"com.android.samsung.",
				"com.sec.android.",
		};
		for (size_t i = 0; i < sizeof(prefix) / sizeof(prefix[0]); i++) {
			if (Utils::startWith(prefix[i], ptr))
				return true;
		}
		return false;
	}

	void applyCfgTemp() {
		for (auto& [uid, info] : infoMap) {
			if (isSystemApp(info.package.c_str()) || whiteListDefault.contains(info.package))
				info.freezeMode = FREEZE_MODE::WHITELIST;
		}

		for (const auto& [uid, cfg] : cfgTemp) {
			auto it = infoMap.find(uid);
			if (it == infoMap.end()) continue;

			it->second.freezeMode = cfg.freezeMode;
			it->second.isTolerant = cfg.isTolerant;
		}

		for (auto& [uid, info] : infoMap) {
			if (whiteListForce.contains(info.package))
				info.freezeMode = FREEZE_MODE::WHITEFORCE;
		}

		if (homePackage.length() > 3) {
			auto it = uidIndex.find(homePackage);
			if (it != uidIndex.end())
				infoMap[it->second].freezeMode = FREEZE_MODE::WHITEFORCE;
		}
	}

	void saveConfig() {
		char buf[1024 * 64];
		size_t len = 0;
		for (const auto& [uid, cfg] : infoMap)
			if (cfg.freezeMode < FREEZE_MODE::WHITEFORCE)
				STRNCAT(buf, len, "%s %d %d\n",
					cfg.package.c_str(),
					static_cast<int>(cfg.freezeMode),
					cfg.isTolerant ? 1 : 0);

		if (Utils::writeString(cfgPath.c_str(), buf, len))
			freezeit.log("保存配置成功");
		else
			freezeit.log("保存配置文件失败: [%s]", cfgPath.c_str());
	}

	void update2xposedByLocalSocket() {
#ifdef __x86_64__
		return;
#endif

		string tmp;
		tmp.reserve(1024L * 16);

		for (int i = 0; i < 40; i++) {
			tmp += to_string(settings[i]);
			tmp += ' ';
		}
		tmp += '\n';

		vector<int> tolerantUids;
		for (const auto& [uid, info] : infoMap) {
			if (info.freezeMode >= FREEZE_MODE::WHITELIST)
				continue;

			tmp += to_string(uid);
			tmp += info.package;
			tmp += ' ';

			if (info.isTolerant)
				tolerantUids.emplace_back(uid);
		}
		tmp += '\n';

		for (const int uid : tolerantUids) {
			tmp += to_string(uid);
			tmp += ' ';
		}
		tmp += '\n';

		if (tmp.empty()) {
			freezeit.log("更新到Xposed异常, 无效数据");
			return;
		}

		for (int i = 0; i < 3; i++) {
			int buff[8];
			int recvLen = Utils::localSocketRequest(XPOSED_CMD::SET_CONFIG, tmp.c_str(),
				tmp.length(), buff, sizeof(buff));

			if (recvLen != 4) {
				freezeit.log("%s() 更新到Xposed 第%d次异常 sendLen[%lu] recvLen[%d] %d:%s",
					__FUNCTION__, i + 1, tmp.length(), recvLen, errno, strerror(errno));

				if (0 < recvLen && recvLen < static_cast<int>(sizeof(buff)))
					freezeit.log("DumpHex: [%s]", Utils::bin2Hex(buff, recvLen).c_str());

				sleep(1);
				continue;
			}

			switch (static_cast<REPLY>(buff[0])) {
			case REPLY::SUCCESS:
				return;
			case REPLY::FAILURE:
				freezeit.log("更新到Xposed失败");
				return;
			default:
				freezeit.log("更新到Xposed 未知回应[%d]", buff[0]);
				return;
			}
		}
		freezeit.log("%s() 工作异常, 请确认LSPosed中冻它勾选系统框架, 然后重启 sendLen[%lu]", __FUNCTION__,
			tmp.length());
	}

	void loadLabelFile() {
		ifstream file(labelPath);

		if (!file.is_open()) {
			freezeit.log("读取应用名称文件失败: [%s]", labelPath.c_str());
			return;
		}

		string line;
		while (getline(file, line)) {
			if (line.length() <= 2) {
				freezeit.log("读取到错误包名: [%s]", line.c_str());
				continue;
			}

			if (isalpha(line[0])) {// package####label
				auto value = Utils::splitString(line, "####");
				if (value.size() != 2) {
					freezeit.log("分割错误: [%s]", line.c_str());
					continue;
				}
				auto it = uidIndex.find(value[0]);
				if (it != uidIndex.end())
					infoMap[it->second].label = value[1];
			}
			else if (isdigit(line[0])) {  // uid label
				int uid = atoi(line.c_str());

				auto it = infoMap.find(uid);
				if (it != infoMap.end() && line.length() > 6)
					it->second.label = line.substr(6);
			}
			else {
				freezeit.log("读取到错误包名: [%s]", line.c_str());
				continue;
			}
		}
		file.close();
	}

	void loadLabel(const map<int, string>& labelList) {
		for (auto& [uid, label] : labelList) {
			auto it = infoMap.find(uid);
			if (it != infoMap.end())
				it->second.label = label;
		}
	}

	void saveLabel() {
		auto fd = open(labelPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0666);
		if (fd < 0) {
			freezeit.log("保存应用名称文件失败: [%s]", labelPath.c_str());
			return;
		}

		string tmp;
		tmp.reserve(1024L * 16);
		for (const auto& [uid, info] : infoMap)
			if (info.package != info.label) {
				tmp += info.package;
				tmp += "####";
				tmp += info.label;
				tmp += '\n';
			}
		write(fd, tmp.c_str(), tmp.length());
		close(fd);
	}


	[[maybe_unused]] void print() {
		string tmp, logContent{ "当前配置:" };

		tmp.clear();
		for (const auto& [uid, info] : infoMap) {
			if (info.freezeMode == FREEZE_MODE::TERMINATE)
				tmp += info.label + " ";
		}
		if (tmp.length())logContent += "\n\n杀死后台: " + tmp;

		tmp.clear();
		for (const auto& [uid, info] : infoMap) {
			if (info.freezeMode == FREEZE_MODE::SIGNAL)
				tmp += info.label + " ";
		}
		if (tmp.length())logContent += "\n\nSIGSTOP冻结: " + tmp;

		tmp.clear();
		for (const auto& [uid, info] : infoMap) {
			if (info.freezeMode == FREEZE_MODE::FREEZER)
				tmp += info.label + " ";
		}
		if (tmp.length())logContent += "\n\nFreezer冻结: " + tmp;

		tmp.clear();
		for (const auto& [uid, info] : infoMap) {
			if (info.freezeMode == FREEZE_MODE::WHITELIST)
				tmp += info.label + " ";
		}
		if (tmp.length())logContent += "\n\n自由后台: " + tmp;

		tmp.clear();
		for (const auto& [uid, info] : infoMap) {
			if (info.freezeMode == FREEZE_MODE::WHITEFORCE)
				tmp += info.label + " ";
		}
		if (tmp.length())logContent += "\n\n自由后台(内置): " + tmp;

		freezeit.log(logContent + "\n");
	}
};
