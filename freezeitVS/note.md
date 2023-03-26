/data/adb/magisk/magisk64 --install-module /sdcard/freezeit_v2.4.2.zip


模块挂载目录
``` sh
$(magisk --path)/.magisk/modules
```

TODO 自启动分析 静态广播拉起？
TODO 监控杀进程信号
TODO 分析后台唤醒
TODO popen() 错误
TODO SIGSTOP会被解冻


objdump 反汇编
/d/AndroidSDK/ndk/21.4.7075529/toolchains/aarch64-linux-android-4.9/prebuilt/windows-x86/aarch64-linux-android-objdump.exe

01-28 12:58:32.598  1696  1834 I am_kill : [0,16059,com.tencent.mobileqq,910,excessive cpu 9100 during 300052 dur=1859349 limit=2]
01-28 13:18:27.969  1696  1736 I am_kill : [0,27165,com.tencent.mobileqq,702,stop com.tencent.mobileqq due to from process:com.miui.securitycenter]
01-28 13:18:27.978  1696  1736 I am_kill : [0,16158,com.tencent.mobileqq:MSF,500,stop com.tencent.mobileqq due to from process:com.miui.securitycenter]
01-28 13:18:27.980  1696  1736 I am_kill : [0,10137,com.tencent.mobileqq:peak,915,stop com.tencent.mobileqq due to from process:com.miui.securitycenter]
01-28 13:18:27.981  1696  1736 I am_kill : [0,14509,com.tencent.mobileqq:tool,935,stop com.tencent.mobileqq due to from process:com.miui.securitycenter]
01-28 14:13:33.724  1696  1834 I am_kill : [0,12908,com.tencent.mobileqq,900,excessive cpu 9970 during 300099 dur=2069196 limit=2]
01-28 15:03:34.615  1696  1834 I am_kill : [0,19954,com.tencent.mobileqq,920,excessive cpu 11510 during 300110 dur=2035621 limit=2]


```c++
void quickSort(int* arr, int len) {
    if (len <= 1) {
        return;
    }
    int pivot = arr[0];
    int i = 0;
    int j = len - 1;
    while (i < j) {
        while (i < j && arr[j] >= pivot) {
            j--;
        }
        arr[i] = arr[j];
        while (i < j && arr[i] <= pivot) {
            i++;
        }
        arr[j] = arr[i];
    }
    arr[i] = pivot;
    quickSort(arr, i);
    quickSort(arr + i + 1, len - i - 1);
}
```



64位ELF头格式相关定义：

```c
typedef uint64_t Elf64_Addr;
typedef uint64_t Elf64_Off;
#define EI_NIDENT 16

typedef struct {
    // 最开头是16个字节的e_ident, 其中包含用以表示ELF文件的字符，以及其他一些与机器无关的信息。
    // 开头的4个字节值固定不变，为0x7f和ELF三个字符。
    unsigned char e_ident[EI_NIDENT];  
    uint16_t e_type;  // 该文件的类型 2字节
    uint16_t e_machine;  // 该程序需要的体系架构 2字节
    uint32_t e_version;  // 文件的版本 4字节
    uint64_t e_entry;  // 程序的入口地址 8字节
    uint64_t e_phoff;  // Program header table 在文件中的偏移量 8字节
    uint64_t e_shoff;  // Section header table 在文件中的偏移量 8字节
    uint32_t e_flags;  // 对IA32而言，此项为0。 4字节
    uint16_t e_ehsize;  // 表示ELF header大小 2字节
    uint16_t e_phentsize;  // 表示Program header table中每一个条目的大小 2字节
    uint16_t e_phnum;  // 表示Program header table中有多少个条目 2字节
    uint16_t e_shentsize;  // 表示Section header table中的每一个条目的大小 2字节
    uint16_t e_shnum;  // 表示Section header table中有多少个条目 2字节
    uint16_t e_shstrndx;  // 包含节名称的字符串是第几个节 2字节
} Elf64_Ehdr;

unsigned char e_ident[EI_NIDENT]; 
uint16_t e_type; uint16_t e_machine; uint32_t e_version;    uint64_t e_entry;
uint64_t e_phoff;                                           uint64_t e_shoff;
uint32_t e_flags;uint16_t e_ehsize;uint16_t e_phentsize;    uint16_t e_phnum;uint16_t e_shentsize; uint16_t e_shnum;uint16_t e_shstrndx;
```

```sh
readelf -e freezeit

readelf -p .comment freezeitWithDebugInfo

String dump of section '.comment':
  [     0]  Android (8490178, based on r450784d) clang version 14.0.6 (https://android.googlesource.com/toolchain/llvm-project 4c603efb0cca074e9238af8b4106c30add4418f6)
  [    9d]  Android (8850317, based on r458507) clang version 15.0.1 (https://android.googlesource.com/toolchain/llvm-project 640d06f2a4ef16626a747d4c45f9bd9a9fdcef4c)
  [   139]  Linker: LLD 14.0.6

```


// Android Freezer 简介
// https://blog.csdn.net/zzz777qqq/article/details/125054175

查看当前localSocket
netstat -apn

所有任务
cat /dev/cpuset/tasks

CPU核心 可用核心
/sys/devices/system/cpu/present

CPU核心 各频率时间
/sys/devices/system/cpu/cpu0/cpufreq/stats/time_in_state

CPU核心 实时频率
/sys/devices/system/cpu/cpu*/cpufreq/scaling_cur_freq

//oom控制
cat /dev/memcg/memory.oom_control

内存信息
sudo lshw -short -C memory

## lmkd 重启
lmkd 
lmkd --reinit

读取全部应用
cat /data/system/packages.list
包名 UID XX 
com.v2ray.ang 10186 0 /data/user/0/com.v2ray.ang default:targetSdkVersion=31 3003 0 2000475 1 com.android.vending
com.microsoft.office.excel 10196 0 /data/user/0/com.microsoft.office.excel default:targetSdkVersion=31 3002,3003 0 2004039667 1 @null
com.android.phone 1001 0 /data/user_de/0/com.android.phone platform:privapp:targetSdkVersion=29 1065,3003,3007,3006 0 32 1 @system


am stack info 0 2 获取桌面
am stack info 1 0 获取当前全屏应用
am stack list | grep -v unknown| grep taskId= | grep visible=true
am stack list 全部界面状态


息屏后有反应  其他时候偶尔也有
inotifyd - /sys/class/wakeup

一些cgroup信息
cat cgroup_info/cgroup.rc

切换应用时 activity时  经常有读写
inotifyd - /dev/binder
inotifyd - /dev/binderfs


是否息屏
/sys/class/leds/lcd-backlight/brightness

/sys/power
该目录是系统中的电源选项，对正在使用的power子系统的描述。这个目录下有几个属性文件可以用于控制整个机器的电源状态，如可以向其中写入控制命令让机器关机/重启等等。
// 获取屏幕状态;
        PowerManager pm = (PowerManager) context.getSystemService(Context.POWER_SERVICE);
        boolean screen = pm.isScreenOn();
os.system("adb shell input keyevent KEYCODE_POWER")
or
os.system('adb shell input keyevent 26')




// const char *pmCmd = "pm list packages -3 | cut -d ':' -f 2";

//最近任务
dumpsys activity recents
dumpsys activity lru


列出第三方应用
pm list packages -3 | cut -d ':' -f 2


// 是否可交互
https://developer.android.com/reference/android/os/PowerManager.html#isInteractive()


// 屏幕状态
dumpsys deviceidle get screen

Intent.ACTION_SCREEN_OFF
Intent.ACTION_SCREEN_ON

visiblePackage.clear
关闭轮询 进入休眠 在系统亮屏前阻塞

Intent.ACTION_SCREEN_ON;
//屏幕被打开之后的广播

//屏幕状态 亮灭
https://developer.android.com/reference/android/view/Display#getState()

//微信语音 视频
com.tencent.mm.plugin.voip.ui.VideoActivity


https://developer.android.com/ndk/reference/group/input
AINPUT_SOURCE_TOUCHSCREEN
https://developer.android.com/ndk/reference/group/input
AKEYCODE_POWER


iptables -t filter -A OUTPUT -m owner --uid-owner 10212 -j DROP
iptables -t filter -D OUTPUT -m owner --uid-owner 10212 -j DROP


只要是来自于172.16.0.0/16网段的都允许访问我本机的172.16.100.1的SSHD服务

分析：首先肯定是在允许表中定义的。因为不需要做NAT地址转换之类的，然后查看我们SSHD服务，在22号端口上，处理机制是接受，对于这个表，需要有一来一回两个规则，如果我们允许也好，拒绝也好，对于访问本机服务，我们最好是定义在INPUT链上，而OUTPUT再予以定义就好。(会话的初始端先定义)，所以加规则就是：
定义进来的： iptables -t filter -A INPUT -s 172.16.0.0/16 -d 172.16.100.1 -p tcp --dport 22 -j ACCEPT
定义出去的： iptables -t filter -A OUTPUT -s 172.16.100.1 -d 172.16.0.0/16 -p tcp --dport 22 -j ACCEPT
将默认策略改成DROP:
iptables -P INPUT DROP
iptables -P OUTPUT DROP
iptables -P FORWARD DROP   

iptables -t filter -A OUTPUT -sport  -j ACCEPT



```
base=/system
export CLASSPATH=/system/framework/am.jar
exec app_process /system/bin com.android.commands.am.Am stack list
```
https://android.googlesource.com/platform/frameworks/base.git/+/android-7.1.1_r22/cmds/am/src/com/android/commands/am/Am.java
#1901

---


---

## 安卓进程状态 

// https://cs.android.com/android/platform/superproject/+/android-12.1.0_r27:frameworks/base/services/core/java/com/android/server/am/ProcessList.java;drc=980f233d2d53512457583df7511e65a2a63269dd;l=4018
dumpsys activity lru

// https://cs.android.com/android/platform/superproject/+/master:frameworks/base/core/java/android/app/ActivityManager.java;drc=7afbbec42f4d30be77a818552aa2201e54c8b798;l=4929
```java
public static String procStateToString(int procState) {
        final String procStateStr;
        switch (procState) {
            case ActivityManager.PROCESS_STATE_PERSISTENT:
                procStateStr = "PER ";
                break;
            case ActivityManager.PROCESS_STATE_PERSISTENT_UI:
                procStateStr = "PERU";
                break;
            case ActivityManager.PROCESS_STATE_TOP:
                procStateStr = "TOP ";
                break;
            case ActivityManager.PROCESS_STATE_BOUND_TOP:
                procStateStr = "BTOP";
                break;
            case ActivityManager.PROCESS_STATE_FOREGROUND_SERVICE:
                procStateStr = "FGS ";
                break;
            case ActivityManager.PROCESS_STATE_BOUND_FOREGROUND_SERVICE:
                procStateStr = "BFGS";
                break;
            case ActivityManager.PROCESS_STATE_IMPORTANT_FOREGROUND:
                procStateStr = "IMPF";
                break;
            case ActivityManager.PROCESS_STATE_IMPORTANT_BACKGROUND:
                procStateStr = "IMPB";
                break;
            case ActivityManager.PROCESS_STATE_TRANSIENT_BACKGROUND:
                procStateStr = "TRNB";
                break;
            case ActivityManager.PROCESS_STATE_BACKUP:
                procStateStr = "BKUP";
                break;
            case ActivityManager.PROCESS_STATE_SERVICE:
                procStateStr = "SVC ";
                break;
            case ActivityManager.PROCESS_STATE_RECEIVER:
                procStateStr = "RCVR";
                break;
            case ActivityManager.PROCESS_STATE_TOP_SLEEPING:
                procStateStr = "TPSL";
                break;
            case ActivityManager.PROCESS_STATE_HEAVY_WEIGHT:
                procStateStr = "HVY ";
                break;
            case ActivityManager.PROCESS_STATE_HOME:
                procStateStr = "HOME";
                break;
            case ActivityManager.PROCESS_STATE_LAST_ACTIVITY:
                procStateStr = "LAST";
                break;
            case ActivityManager.PROCESS_STATE_CACHED_ACTIVITY:
                procStateStr = "CAC ";
                break;
            case ActivityManager.PROCESS_STATE_CACHED_ACTIVITY_CLIENT:
                procStateStr = "CACC";
                break;
            case ActivityManager.PROCESS_STATE_CACHED_RECENT:
                procStateStr = "CRE ";
                break;
            case ActivityManager.PROCESS_STATE_CACHED_EMPTY:
                procStateStr = "CEM ";
                break;
            case ActivityManager.PROCESS_STATE_NONEXISTENT:
                procStateStr = "NONE";
                break;
            default:
                procStateStr = "??";
                break;
        }
        return procStateStr;
    }

//https://cs.android.com/android/platform/superproject/+/master:out/soong/.intermediates/frameworks/base/framework-minus-apex/android_common/xref35/srcjars.xref/android/app/ProcessStateEnum.java;l=10
public @interface ProcessStateEnum {
  /** @hide Not a real process state. */
  public static final int UNKNOWN = -1;
  /** @hide Process is a persistent system process. */
  public static final int PERSISTENT = 0;
  /** @hide Process is a persistent system process and is doing UI. */
  public static final int PERSISTENT_UI = 1;
  /**
   * @hide Process is hosting the current top activities.  Note that this covers
   * all activities that are visible to the user.
   */
  public static final int TOP = 2; // 顶层
  /** @hide Process is bound to a TOP app. */
  public static final int BOUND_TOP = 3;
  /** @hide Process is hosting a foreground service. */
  public static final int FOREGROUND_SERVICE = 4; //常驻状态栏
  /** @hide Process is hosting a foreground service due to a system binding. */
  public static final int BOUND_FOREGROUND_SERVICE = 5;
  /** @hide Process is important to the user, and something they are aware of. */
  public static final int IMPORTANT_FOREGROUND = 6; // 悬浮窗
  /** @hide Process is important to the user, but not something they are aware of. */
  public static final int IMPORTANT_BACKGROUND = 7;
  /** @hide Process is in the background transient so we will try to keep running. */
  public static final int TRANSIENT_BACKGROUND = 8;
  /** @hide Process is in the background running a backup/restore operation. */
  public static final int BACKUP = 9;
  /**
   * @hide Process is in the background running a service.  Unlike oom_adj, this level
   * is used for both the normal running in background state and the executing
   * operations state.
   */
  public static final int SERVICE = 10;
  /**
   * @hide Process is in the background running a receiver.   Note that from the
   * perspective of oom_adj, receivers run at a higher foreground level, but for our
   * prioritization here that is not necessary and putting them below services means
   * many fewer changes in some process states as they receive broadcasts.
   */
  public static final int RECEIVER = 11;
  /** @hide Same as {@link #PROCESS_STATE_TOP} but while device is sleeping. */
  public static final int TOP_SLEEPING = 12;
  /**
   * @hide Process is in the background, but it can't restore its state so we want
   * to try to avoid killing it.
   */
  public static final int HEAVY_WEIGHT = 13;
  /** @hide Process is in the background but hosts the home activity. */
  public static final int HOME = 14;
  /** @hide Process is in the background but hosts the last shown activity. */
  public static final int LAST_ACTIVITY = 15;
  /** @hide Process is being cached for later use and contains activities. */
  public static final int CACHED_ACTIVITY = 16;
  /**
   * @hide Process is being cached for later use and is a client of another cached
   * process that contains activities.
   */
  public static final int CACHED_ACTIVITY_CLIENT = 17;
  /**
   * @hide Process is being cached for later use and has an activity that corresponds
   * to an existing recent task.
   */
  public static final int CACHED_RECENT = 18;
  /** @hide Process is being cached for later use and is empty. */
  public static final int CACHED_EMPTY = 19;
  /** @hide Process does not exist. */
  public static final int NONEXISTENT = 20;
}

```



```c
#include <unistd.h>

int sysconf(_SC_NPROCESSORS_CONF);/* 返回系统可以使用的核数，但是其值会包括系统中禁用的核的数目，因 此该值并不代表当前系统中可用的核数 */
int sysconf(_SC_NPROCESSORS_ONLN);/* 返回值真正的代表了系统当前可用的核数 */

/* 以下两个函数与上述类似 */
#include <sys/sysinfo.h>
int get_nprocs_conf (void);/* 可用核数 */
int get_nprocs (void);/* 真正的反映了当前可用核数 */


#define _GNU_SOURCE             /* See feature_test_macros(7) */
#include <sched.h>
/* 设置进程号为pid的进程运行在mask所设定的CPU上
 * 第二个参数cpusetsize是mask所指定的数的长度
 * 通常设定为sizeof(cpu_set_t)

 * 如果pid的值为0,则表示指定的是当前进程 
 */
int sched_setaffinity(pid_t pid, size_t cpusetsize, cpu_set_t *mask);
int sched_getaffinity(pid_t pid, size_t cpusetsize, cpu_set_t *mask);/* 获得pid所指示的进程的CPU位掩码,并将该掩码返回到mask所指向的结构中 */
```

使用 ioctl 与 binder 驱动通信
https://www.h5w3.com/243638.html


// 神经网络 https://developer.android.com/ndk/guides/neuralnetworks

## 文件系统 一些特殊文件的作用
// https://source.android.google.cn/docs/core/architecture/kernel/reqs-interfaces



# 跳转到大帅批的主页
am start -n com.coolapk.market/.view.userv9.UserSpaceV9Activity --es key_uid 1212220

# 小窗
am start --windowingMode 5 -n com.coolapk.market/.view.userv9.UserSpaceV9Activity --es key_uid 1212220


## 监控 /dev/binderfs/binder_logs

/*
binder
内核 binder transaction_log
/dev/binderfs/binder_logs/transaction_log
/sys/kernel/debug/binder/transaction_log
https://cs.android.com/android/kernel/superproject/+/common-android12-5.10:common/drivers/android/binder.c;drc=8d868467814e79d4770c276f954e5dd928401da6;l=6153
*/

// https://github.com/LineageOS/android_kernel_xiaomi_sdm845/blob/lineage-19.1/drivers/android/binder.c
// https://github.com/MiCode/Xiaomi_Kernel_OpenSource/blob/munch-s-oss/drivers/android/binder.c

if (api == "30")
stateFilePath = "/dev/binderfs/binder_logs/state";
else
stateFilePath = "/sys/kernel/debug/binder/state";





---

## CPU占用率计算

CPU使用率 = (1 - (new_空闲时间 - old_空闲时间) / (new_总CPU时间 - old_总CPU时间)) * 100
总CPU时间：user + nice + system + idle + iowait + softirq + steal + guest + guest_nice
空闲时间：idle

---

## 网络 socket
```sh
# 反查IP 断开网络
cd /proc/`pidof com.tencent.mobileqq:MSF`/fd
ls -al | grep socket
# **** socket:[mumberInode]
cat /proc/net/tcp


tcpkill -9 port ftp &>/dev/null
tcpkill -9 host 192.168.10.30 &>/dev/null
tcpkill -9 port 53 and port 8000 &>/dev/null
tcpkill -9 net 192.168.10 &>/dev/null
tcpkill -9 net 192.168.10 and port 22 &>/dev/null
```


---

## clang llvm
-O0, -O1, -O2, -O3, -Ofast, -Os, -Oz, -Og, -O, -O4
指定要使用的优化级别。

-O0 表示 "不优化"：这个级别的编译速度最快，产生的代码最容易调试。
-O1 介于-O0和-O2之间。
-O2 适度的优化水平，可以实现大多数优化。
-O3 与-O2相同，只是它启用了需要较长时间执行的优化，或可能产生较大的代码（试图使程序运行更快）。
-Ofast 启用-O3中的所有优化，以及其他可能违反语言标准的激进优化。
-Os 像-O2一样，有额外的优化以减少代码大小。
-Oz 像-O(因此也是-O2)，但进一步减少代码大小。
-Og 像 -O1。在未来的版本中，这个选项可能会禁用不同的优化，以提高调试性。
-O 相当于 -O1。
-O4及以上目前等同于-O3

---

## swap

```sh
# swap开启
fallocate -l 16G /data/swapfile
chmod 0666 /data/swapfile
mkswap swapfile
swapon swapfile
```



- 画大饼🤩(不是), 头发不保🥵: 
  - 1.未来的未来, 计划研究Framework, ActivityManager等底层, 重塑Activity生命周期, 主要是[ OnPause/OnResume ]等机制。 解决[ 重回应用 ]再次显示广告问题(危)。
  - 2.未来的未来, 计划加入基于机器学习的CPU/GPU调度(也许是BP?), 智能学习用户日常使用习惯、应用进程CPU算力需求(频率变化特征)等等, 实现让每一个核心均工作在其刚刚好的频率上, 降低空转比例。
      依靠学习到的频率变化特征, 智能预测短时未来的高/低算力需求, 在恰当时机变频, 学习到的调度参数将会是当前用户/当前设备独一无二的专有调度, 直接自适应本机所有应用场景, 不需要省电/平衡/性能等繁琐的模式, 同时覆盖系统的省电、性能、游戏模式等等。
      说人话就是, 根据CPU频率变化特征及其他等信息, 一旦识别到你即将开团, 将会提前CPU/GPU升频, 降低其他非必须进程的运行优先级, 让你满屏技能乱飞时尽量减少掉帧, 而看视频、小说等使用时间长, 操作少的场景况就智能降频, 省电处理。
      应对高温场景允许短时间突破温控, 而不是死板的降频, 对于大型游戏这类持续性CPU/GPU高算力需求可能有额外优化, 目标是稳帧。
      神经网络将进行低功耗化模型改造, 尽可能降低自身复杂度, 使用硬件加速(要求SOC必须支持NPU)。
  - 3.以上大饼看看热闹就好, 别期待。




{ am; } > /sdcard/amHelp.txt
{ pm; } > /sdcard/pmHelp.txt

busybox ps -o user -o pid -o args | grep u0_a

am stack info <WINDOWING_MODE> <ACTIVITY_TYPE>

https://android.googlesource.com/platform/frameworks/base/+/master/core/java/android/app/WindowConfiguration.java#94

## <WINDOWING_MODE>

```java
/** Windowing mode is currently not defined. */
public static final int WINDOWING_MODE_UNDEFINED = 0;
/** Occupies the full area of the screen or the parent container. */
public static final int WINDOWING_MODE_FULLSCREEN = 1;
/** Always on-top (always visible). of other siblings in its parent container. */
public static final int WINDOWING_MODE_PINNED = 2;
/** The primary container driving the screen to be in split-screen mode. */
//  Remove once split-screen is migrated to wm-shell.
public static final int WINDOWING_MODE_SPLIT_SCREEN_PRIMARY = 3;
/**
    * The containers adjacent to the {@link #WINDOWING_MODE_SPLIT_SCREEN_PRIMARY} container in
    * split-screen mode.
    * NOTE: Containers launched with the windowing mode with APIs like
    * {@link ActivityOptions#setLaunchWindowingMode(int)} will be launched in
    * {@link #WINDOWING_MODE_FULLSCREEN} if the display isn't currently in split-screen windowing
    * mode
    */
//  Remove once split-screen is migrated to wm-shell.
public static final int WINDOWING_MODE_SPLIT_SCREEN_SECONDARY = 4;
/** Can be freely resized within its parent container. */
//  Remove once freeform is migrated to wm-shell.
public static final int WINDOWING_MODE_FREEFORM = 5;
/** Generic multi-window with no presentation attribution from the window manager. */
public static final int WINDOWING_MODE_MULTI_WINDOW = 6;
```

## <ACTIVITY_TYPE>
```java

/** Activity type is currently not defined. */
public static final int ACTIVITY_TYPE_UNDEFINED = 0;
/** Standard activity type. Nothing special about the activity... */
public static final int ACTIVITY_TYPE_STANDARD = 1;
/** Home/Launcher activity type. */
public static final int ACTIVITY_TYPE_HOME = 2;
/** Recents/Overview activity type. There is only one activity with this type in the system. */
public static final int ACTIVITY_TYPE_RECENTS = 3;
/** Assistant activity type. */
public static final int ACTIVITY_TYPE_ASSISTANT = 4;
/** Dream activity type. */
public static final int ACTIVITY_TYPE_DREAM = 5;
```


```c++

      char buf[100];
      snprintf(buf, 100, "cmd appops set %s WAKE_LOCK ignore", package.c_str());
      system(buf);

      
  {"com.netease.cloudmusic", 1},       //网易云
  {"com.tencent.qqmusic", 1},          // QQ音乐
  {"com.kugou.android", 1},            //酷狗
  {"cn.kuwo.player", 1},               //酷我
  {"com.salt.music", 1},               //椒盐
  {"com.spotify.music", 1},            // Spotify
  {"cn.toside.music.mobile", 1},       // LX Music
  {"cmccwm.mobilemusic", 1},           //咪咕音乐
  {"com.douban.radio", 1},             //豆瓣FM
  {"com.yibasan.lizhifm", 1},          //荔枝
  {"com.tencent.qqmusicpad", 1},       // QQ音乐HD
  {"cn.missevan", 1},                  //猫耳FM
  {"com.changba", 1},                  //唱吧
  {"com.ximalaya.ting.android", 1},    //喜马拉雅
  {"com.ximalaya.ting.lite", 1},       //喜马拉雅
  {"com.xs.fm", 1},                    //番茄畅听
  {"com.dragon.read", 1},              //番茄免费小说
  {"com.youku.phone", 1},              //优酷视频
  {"com.tencent.qqlive", 1},           //腾讯视频
  {"com.ss.android.article.video", 1}, //西瓜视频
  {"tv.danmaku.bili", 1},              //哔哩哔哩
  {"com.bilibili.app.in", 1},          //哔哩哔哩Play
  {"com.qiyi.video", 1},               //爱奇艺
  {"com.qiyi.video.lite", 1},          //爱奇艺极速版
  {"tv.acfundanmaku.video", 1},        // AcFun
  {"com.google.android.youtube", 1},   // Youtube
  {"com.netflix.mediaclient", 1},      // Netflix
  {"com.miui.player", 1},              //小米音乐
  {"com.xiaohei.xiaoheiapps", 1},      //大师兄影视
  {"com.apple.android.music", 1},      // Apple Music
  {"com.duowan.kiwi", 1},              //虎牙直播
  {"bubei.tingshu", 1},                //懒人听书
  {"io.legado.app.release", 1},        //阅读


  

  {"com.baidu.input", 3},                      //百度输入法
  {"com.baidu.input_huawei", 3},               //百度输入法华为版
  {"com.baidu.input_mi", 3},                   //百度输入法小米版
  {"com.baidu.input_oppo", 3},                 //百度输入法OPPO版
  {"com.baidu.input_vivo", 3},                 //百度输入法VIVO版
  {"com.baidu.input_yijia", 3},                //百度输入法一加版

  {"com.sohu.inputmethod.sogou", 3},           //搜狗输入法
  {"com.sohu.inputmethod.sogou.xiaomi", 3},    //搜狗输入法小米版
  {"com.sohu.inputmethod.sogou.meizu", 3},     //搜狗输入法魅族版
  {"com.sohu.inputmethod.sogou.nubia", 3},     //搜狗输入法nubia版
  {"com.sohu.inputmethod.sogou.chuizi", 3},    //搜狗输入法chuizi版
  {"com.sohu.inputmethod.sogou.moto", 3},      //搜狗输入法moto版
  {"com.sohu.inputmethod.sogou.zte", 3},       //搜狗输入法中兴版
  {"com.sohu.inputmethod.sogou.samsung", 3},   //搜狗输入法samsung版
  {"com.sohu.input_yijia", 3},                 //搜狗输入法一加版

  {"com.iflytek.inputmethod", 3},              //讯飞输入法
  {"com.iflytek.inputmethod.miui", 3},         //讯飞输入法小米版
  {"com.iflytek.inputmethod.googleplay", 3},   //讯飞输入法googleplay版
  {"com.iflytek.inputmethod.smartisan", 3},    //讯飞输入法smartisan版
  {"com.iflytek.inputmethod.oppo", 3},         //讯飞输入法oppo版
  {"com.iflytek.inputmethod.oem", 3},          //讯飞输入法oem版
  {"com.iflytek.inputmethod.custom", 3},       //讯飞输入法custom版
  {"com.iflytek.inputmethod.blackshark", 3},   //讯飞输入法blackshark版
  {"com.iflytek.inputmethod.zte", 3},          //讯飞输入法zte版

  {"com.tencent.qqpinyin", 3},                 // QQ拼音输入法
  {"com.google.android.inputmethod.latin", 3}, //谷歌Gboard输入法
  {"com.touchtype.swiftkey", 3},               //微软swiftkey输入法
  {"com.touchtype.swiftkey.beta", 3},          //微软swiftkeyBeta输入法
  {"im.weshine.keyboard", 3},                  // KK键盘输入法
  {"com.komoxo.octopusime", 3},                //章鱼输入法
  {"com.qujianpan.duoduo", 3},                 //见萌输入法
  {"com.lxlm.lhl.softkeyboard", 3},            //流行输入法
  {"com.jinkey.unfoldedime", 3},               //不折叠输入法
  {"com.iflytek.inputmethods.DungkarIME", 3},  //东噶藏文输入法
  {"com.oyun.qingcheng", 3},                   //奥云蒙古文输入法
  {"com.ziipin.softkeyboard", 3},              // Badam维语输入法
  {"com.kongzue.secretinput", 3},              // 密码键盘

  
  const vector<string> androidVer = {
      "Android Unknown", // SDK0
      "Android 1",       // SDK1
      "Android 1.1",     // SDK2
      "Android 1.5",     // SDK3
      "Android 1.6",     // SDK4
      "Android 2",       // SDK5
      "Android 2.0.1",   // SDK6
      "Android 2.1",     // SDK7
      "Android 2.2",     // SDK8
      "Android 2.3",     // SDK9
      "Android 2.3.3",   // SDK10
      "Android 3.0",     // SDK11
      "Android 3.1",     // SDK12
      "Android 3.2",     // SDK13
      "Android 4.0",     // SDK14
      "Android 4.0.3",   // SDK15
      "Android 4.1",     // SDK16
      "Android 4.2",     // SDK17
      "Android 4.3",     // SDK18
      "Android 4.4",     // SDK19
      "Android 4.4 w",   // SDK20
      "Android 5.0",     // SDK21
      "Android 5.1",     // SDK22
      "Android 6.0",     // SDK23
      "Android 7.0",     // SDK24
      "Android 7.1",     // SDK25
      "Android 8.0",     // SDK26
      "Android 8.1",     // SDK27
      "Android 9",       // SDK28
      "Android 10",      // SDK29
      "Android 11",      // SDK30
      "Android 12",      // SDK31
      "Android 12L",     // SDK32
      "Android 13",      // SDK33
      "Android 14",      // SDK34
  };

```

```java

/* processCurBroadcastLocked 处理静态注册的BroadCastReceiver
    * SDK26 ~ SDK33 (Android 8.0-13/O-T) BroadcastQueue.java : processCurBroadcastLocked()
    * SourceCode frameworks/base/services/core/java//com/android/server/am/BroadcastQueue.java
    * link https://cs.android.com/android/platform/superproject/+/master:frameworks/base/services/core/java/com/android/server/am/BroadcastQueue.java;l=298
    * Param private final void processCurBroadcastLocked(BroadcastRecord r, ProcessRecord app)
    */
XC_MethodHook processCurBroadcastLockedHook = new XC_MethodHook() {
    public void beforeHookedMethod(MethodHookParam param) {
        Object[] args = param.args;

        // 静态广播
        String callerPackage = (String) XposedHelpers.getObjectField(args[0], Enum.Field.callerPackage);
        Object processRecord = args[1];
        ApplicationInfo appInfo = (ApplicationInfo) XposedHelpers.getObjectField(args[1], Enum.Field.info);

        if (processRecord == null || appInfo == null) {
//                    XposedHelpers.setObjectField(receiverList, Enum.Field.app, null);

//                    String ss = receiverList.size() + ":1" +
//                            (processRecord == null ? "0" : "1") +
//                            (appInfo == null ? "0" : "1");
//                    log("Clear broadcast of [" + callerPackage + "] to [" + receiverPackage + "]: size:" + ss);
            return;
        }
    }
};

try {
    XposedHelpers.findAndHookMethod(Enum.Class.BroadcastQueue, lpParam.classLoader, Enum.Method.processCurBroadcastLocked,
            Enum.Class.BroadcastRecord, Enum.Class.ProcessRecord, processCurBroadcastLockedHook);
log("Freezeit hook BroadcastQueue: processCurBroadcastLocked success");
} catch (Exception e) {
    log("Freezeit hook BroadcastQueue fail:" + e);
}




https://cs.android.com/android/platform/superproject/+/master:frameworks/base/services/core/java/com/android/server/am/ActivityManagerService.java
Object activityManagerService = XposedHelpers.getObjectField(receiverList, Enum.Field.owner);

Class<?> clazz = activityManagerService.getClass();
Method method = null;

while (clazz != null) {
    method = XposedHelpers.findMethodExactIfExists(clazz, Enum.Method.isAppForeground, int.class);
    if (method != null) {
        break;
    } else {
        clazz = clazz.getSuperclass();
    }
}

int uid = XposedHelpers.getIntField(receiverList, Enum.Field.uid);
boolean isAppForeground = false;
try {
    isAppForeground = (method != null) &&
            (boolean) XposedHelpers.findMethodBestMatch(clazz, Enum.Method.isAppForeground, uid).invoke(activityManagerService, uid);
} catch (IllegalAccessException | InvocationTargetException e) {
    log("find isAppForeground() failed");
}

if (isAppForeground) {
    log("skip foreground:" + receiverPackage);
    return;
}


Object activityManagerService = XposedHelpers.getObjectField(receiverList, Enum.Field.owner);
Class<?> clazz = activityManagerService.getClass();
if(clazz != null) {

    while (clazz.getSuperclass() != null)
        clazz = clazz.getSuperclass();

    Object mProcessList = XposedHelpers.findField(clazz, Enum.Field.mProcessList);;
    Object mActiveUids = (mProcessList == null) ? null : XposedHelpers.getObjectField(mProcessList, Enum.Field.mActiveUids);
    Object uidRec = (mActiveUids == null) ? null : XposedHelpers.callMethod(mActiveUids, "get", appInfo.uid);

    int CurProcState;
    if (uidRec == null || (boolean) XposedHelpers.callMethod(uidRec, "isIdle")) {
        CurProcState = 100;
    } else
        CurProcState = (int) XposedHelpers.callMethod(uidRec, "getCurProcState");
    log("CurProcState:[" + CurProcState + "] " + receiverPackage);

    // 正在前台显示
    // https://cs.android.com/android/platform/superproject/+/master:frameworks/base/core/java/android/app/ActivityManager.java;l=540
    if (CurProcState <= 2) {
        log("skip foreground:" + receiverPackage);
        return;
    }
}
```

```c

#if TIME_LIMIT
  const char* endStr = "2022-11-11 00:00:00";
  const time_t end = timeFormat2Timestamp(endStr);
  time_t now = time(nullptr);

  if (now > end) {
    module["description"] = "当前版本已经过期, 模块已停止工作, 请更新最新版本。[Q群](781222669)";
    if (!module.save2file()) fprintf(stderr, "写入 [moduleInfo] 失败");
    exit(11);
  }

  int len = end - now;
  int day = len / 86400;
  len %= 86400;
  int hour = len / 3600;
  len %= 3600;
  int mini = len / 60;
  int sec = len % 60;
  if (day < 180) {
    freezeit.log("内测到期时间: %s", endStr);
    freezeit.log("内测剩余时间: %d天%02d时%02d分%02d秒", day, hour, mini, sec);
  } else {
    freezeit.log("冻它模块 长期版");
  }
#endif
```


```java

    new Thread(this::test).start();


    void test() {
        final String TAG = "setTest";
        int sum = 0; //凑数的，无用
        long st;
        long[] dr = new long[4];

        final int SET_SIZE = 4;
        final int TIMES = 7000;

        var a = new HashSet<Integer>(400);
        var b = new TreeSet<Integer>();
        var c = new XpUtils.VectorSet(400);
        var d = new XpUtils.BucketSet();
        var r = new Random();

        for (int i = 0; i < SET_SIZE; i++) {
            int n = 10000 + (SET_SIZE < 50 ? r.nextInt(500) : i);
            a.add(n);
            b.add(n);
            c.add(n);
            d.add(n);
        }

        Log.e(TAG, "test size: " + a.size());

        // 49Million次 contain 平均耗时，毫秒, 时长很稳定。骁龙845定频，最高频
        // HashSet TreeSet arraySet bucketSet
        // [1737, 3443,  未测, 183]  270个元素
        // [1515, 2045, 1349, 176]  4个元素
        // [1460, 1734, 963, 178]   2个元素
        // [1478, 1746, 717, 170]   1个元素
        for (int tt = 0; tt < 10; tt++) { //连续测试10次

            st = System.currentTimeMillis();
            for (int i = 0; i < TIMES; i++)
                for (int j = 0; j < TIMES; j++)
                    if (a.contains(i + j)) sum++;
            dr[0] = System.currentTimeMillis() - st;

            st = System.currentTimeMillis();
            for (int i = 0; i < TIMES; i++)
                for (int j = 0; j < TIMES; j++)
                    if (b.contains(i + j)) sum++;
            dr[1] = System.currentTimeMillis() - st;

            st = System.currentTimeMillis();
            for (int i = 0; i < TIMES; i++)
                for (int j = 0; j < TIMES; j++)
                    if (c.contains(j)) sum++;
            dr[2] = System.currentTimeMillis() - st;

            st = System.currentTimeMillis();
            for (int i = 0; i < TIMES; i++)
                for (int j = 0; j < TIMES; j++)
                    if (d.contains(i + j)) sum++;
            dr[3] = System.currentTimeMillis() - st;

            Log.e(TAG, "duration: " + Arrays.toString(dr) + ((sum == 0) ? "0" : ""));
        }
    }
```