/* Freezeit: 冻它模块
 * Copyright (c) 2023 JARK006
 * 
 * 命令行编译：
 * clang++.exe --target=aarch64-none-linux-android29 --sysroot=D:/AndroidSDK/ndk/25.2.9519653/toolchains/llvm/prebuilt/windows-x86_64/sysroot -std=c++20 -static -s -Ofast -Wall -Wextra -Wshadow -fno-exceptions -fno-rtti -DNDEBUG -fPIE -Iinclude src/main.cpp -o build/freezeit
 * 
 * 主线程8MiB  子线程 栈深约 1016Kib
 * 
 */

#include "freezeit.hpp"
#include "settings.hpp"
#include "managedApp.hpp"
#include "systemTools.hpp"
#include "doze.hpp"
#include "freezer.hpp"
#include "server.hpp"

const int TEST_CODE = 0;

void test();

int main(int argc, char **argv) {

    if (TEST_CODE) {
        test();
        exit(0);
    }

    //先获取模块目录，下面Init开启守护线程后就不能获取了
    char fullPath[1024] = {};
    auto pathPtr = realpath(argv[0], fullPath); 

    Utils::Init();

    Freezeit freezeit(argc, string(pathPtr));
    Settings settings(freezeit);
    ManagedApp managedApp(freezeit, settings);
    SystemTools systemTools(freezeit, settings);
    Doze doze(freezeit, settings, managedApp, systemTools);
    Freezer freezer(freezeit, settings, managedApp, systemTools, doze);
    Server server(freezeit, settings, managedApp, systemTools, doze, freezer);

    // 424720 Bytes
    //constexpr int size = sizeof(Freezeit) + sizeof(Settings) + sizeof(ManagedApp) + sizeof(SystemTools)
    //    + sizeof(Doze) + sizeof(Freezer) + sizeof(Server);

    sleep(3600 * 24 * 365);//放年假
    return 0;
}

void test() {
    printf("Test\n");
}
