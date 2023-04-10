/* Freezeit: 冻它模块
 * Copyright (c) 2023 JARK006
 * 
 * 命令行编译：
 * clang++.exe --target=aarch64-none-linux-android29 --sysroot=D:/AndroidSDK/ndk/25.2.9519653/toolchains/llvm/prebuilt/windows-x86_64/sysroot -std=c++20 -static -s -Ofast -Wall -Wextra -Wshadow -fno-exceptions -fno-rtti -DNDEBUG -fPIE -Iinclude src/main.cpp -o build/freezeit
 * 
 * 主线程8MiB  子线程 栈深约 1016Kib
 * 
 * 在左下角 Build Variants 选择 Release
 * 在任意 cpp头文件或源码文件中，菜单 Build -> Recompile "xxx"
 * 编译输出 module\\build\intermediates\cmake\release\obj\arm64-v8a
 * 
 */

#include "freezeit.hpp"
#include "settings.hpp"
#include "managedApp.hpp"
#include "systemTools.hpp"
#include "doze.hpp"
#include "freezer.hpp"
#include "server.hpp"

int main(int argc, char **argv) {

    Utils::Init();

    Freezeit freezeit(argc, argv[0]);
    Settings settings(freezeit);
    ManagedApp managedApp(freezeit, settings);
    SystemTools systemTools(freezeit, settings);
    Doze doze(freezeit, settings, managedApp, systemTools);
    Freezer freezer(freezeit, settings, managedApp, systemTools, doze);
    Server server(freezeit, settings, managedApp, systemTools, doze, freezer);

    sleep(3600 * 24 * 365);//放年假
    return 0;
}
