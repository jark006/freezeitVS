$ErrorActionPreference = 'Stop'

function log
{
    [CmdletBinding()]
    Param
    (
        [Parameter(Mandatory = $true, Position = 0)]
        [string]$LogMessage
    )
    Write-Output ("[{0}] {1}" -f (Get-Date), $LogMessage)
}

$windowsToolchainsDir = "D:/AndroidSDK/ndk/25.2.9519653/toolchains/llvm/prebuilt/windows-x86_64/bin"
$clang = "${windowsToolchainsDir}/clang++.exe"
#$target = "--target=aarch64-none-linux-android29"
$target = "--target=x86_64-none-linux-android29"
$sysroot = "--sysroot=D:/AndroidSDK/ndk/25.2.9519653/toolchains/llvm/prebuilt/windows-x86_64/sysroot"
$cppFlags = "-std=c++20 -static -s -Ofast -Wall -Wextra -Wshadow -fno-exceptions -fno-rtti -DNDEBUG -fPIE"

log "Compiler..."
& $clang $target $sysroot $cppFlags.Split(' ') -I. main.cpp -o ./X64/freezeit

log "Done"
