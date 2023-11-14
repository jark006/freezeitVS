$ErrorActionPreference = 'Stop'

function log($logContent){
    Write-Host ("[{0}] {1}" -f (Get-Date), $logContent)
}

function abort($logContent){
    Write-Host ("[{0}] {1}" -f (Get-Date), $logContent) -ForegroundColor:Red 
    exit -1
}

$ndkPath = "D:/AndroidSDK/ndk/android-ndk-r26"
$clang = "${ndkPath}/toolchains/llvm/prebuilt/windows-x86_64/bin/clang++.exe"
$sysroot = "--sysroot=${ndkPath}/toolchains/llvm/prebuilt/windows-x86_64/sysroot"
$cppFlags = "-std=c++20 -static -s -Ofast -Wall -Wextra -Wshadow -fno-exceptions -fno-rtti -DNDEBUG -fPIE"

$target = "--target=x86_64-none-linux-android31"
log "Compiler X64 ..."
& $clang $target $sysroot $cppFlags.Split(' ') -Iinclude src/main.cpp -o magisk/freezeitX64
if (-not$?)
{
    abort "Compiler fail"
}

log "Done"
