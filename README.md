# Freezeit 冻它

**[面具模块]** 实现部分墓碑机制，自动暂停后台进程的运行。

**[MagiskModule]** Implement a partial tombstone mechanism to automatically suspend background processes.

### 开发说明

1. 使用 `VisualStudio2022` 进行开发，在 `VisualStudio Installer` 中安装 `使用C++的移动开发` 组件，如果不安装，`智能提示` 将无法工作。
1. 在 `build_pack.ps1` 第18/19行 设置 NDK工具链路径 `$ndkPath` 和 zip打包路径 `$releaseDir`, 然后执行 `build_pack.ps1` 即可直接编译及打包 (项目生成功能已设为执行 build_pack.ps1，所以通过菜单 “生成-重新生成freezeitVS” 即可进行编译打包)。
1. 其中 `$ndkPath` 可以使用第一步安装的组件，默认路径 `C:\Microsoft\AndroidNDK\android-ndk-r23c`，也可以使用AndroidSDK里的NDK(如果你有的话)，也可以使用单独的NDK工具链（[下载地址](https://developer.android.com/ndk/downloads)）。

### 相关链接

1. [项目开源地址](https://github.com/jark006/freezeitVS)

1. [管理器开源地址](https://github.com/jark006/freezeitapp)

1. [模块包发布地址](https://github.com/jark006/freezeitRelease)

### 其他链接

[教程 Tutorials](https://jark006.github.io/FreezeitIntroduction/) |
[酷安 @JARK006](https://www.coolapk.com/u/1212220) |
[QQ频道 冻它模块](https://qun.qq.com/qqweb/qunpro/share?_wv=3&_wwv=128&appChannel=share&inviteCode=1W6opB7&appChannel=share&businessType=9&from=246610&biz=ka) |
[Telegram Group](https://t.me/+sjDX1oTk31ZmYjY1) |
[蓝奏云 密码: dy6i](https://jark006.lanzout.com/b017oz9if) 

---

