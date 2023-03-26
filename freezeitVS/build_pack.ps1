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
$id = Get-Content magisk/module.prop | Where-Object { $_ -match "id=" }
$id = $id.split('=')[1]
$version = Get-Content magisk/module.prop | Where-Object { $_ -match "version=" }
$version = $version.split('=')[1]
$versionCode = Get-Content magisk/module.prop | Where-Object { $_ -match "versionCode=" }
$versionCode = $versionCode.split('=')[1]
$zipFile = "${id}_v${version}.zip"

$windowsToolchainsDir = "D:/AndroidSDK/ndk/25.2.9519653/toolchains/llvm/prebuilt/windows-x86_64/bin"
$clang = "${windowsToolchainsDir}/clang++.exe"
$target = "--target=aarch64-none-linux-android29"
$sysroot = "--sysroot=D:/AndroidSDK/ndk/25.2.9519653/toolchains/llvm/prebuilt/windows-x86_64/sysroot"
$cppFlags = "-std=c++20 -static -s -Ofast -Wall -Wextra -Wshadow -fno-exceptions -fno-rtti -DNDEBUG -fPIE"

log "Compiler..."
& $clang $target $sysroot $cppFlags.Split(' ') -I. main.cpp -o magisk/${id}

Copy-Item changelog.txt magisk -force


$releaseDir = "D:/Project-github/freezeitRelease"
if ((Test-Path $releaseDir) -ne "True")
{
    log "None Path: $releaseDir"
    exit
}

log "Packing... $zipFile"
& ./7za.exe a "${releaseDir}/${zipFile}" ./magisk/* | Out-Null
if (-not$?)
{
    log "Pack fail"
    exit
}

# https://github.com/jark006/freezeitRelease/raw/main/$zipFile
# https://github.com/jark006/freezeitRelease/raw/main/changelog.txt

# https://raw.githubusercontent.com/jark006/freezeitRelease/main/$zipFile
# https://raw.githubusercontent.com/jark006/freezeitRelease/main/changelog.txt
# https://raw.githubusercontent.com/jark006/freezeitRelease/main/freezeit_v2.2.17.zip

# https://raw.fastgit.org/jark006/freezeitRelease/main/$zipFile
# https://raw.fastgit.org/jark006/freezeitRelease/main/changelog.txt

# https://cdn.jsdelivr.net/gh/jark006/freezeitRelease/$zipFile
# https://cdn.jsdelivr.net/gh/jark006/freezeitRelease/changelog.txt

# https://gitee.com/jark006/freezeit-release/releases/download/${version}/$zipFile

# https://ghproxy.com/https://raw.githubusercontent.com/jark006/freezeitRelease/main/$zipFile
# https://ghproxy.com/https://raw.githubusercontent.com/jark006/freezeitRelease/main/changelog.txt

log "Creating... update json"
$jsonContent = "{
    `"version`": `"$version`",
    `"versionCode`": $versionCode,
    `"zipUrl`": `"https://ghproxy.com/https://raw.githubusercontent.com/jark006/freezeitRelease/main/$zipFile`",
    `"changelog`": `"https://ghproxy.com/https://raw.githubusercontent.com/jark006/freezeitRelease/main/changelog.txt`"`n}"
$jsonContent > ${releaseDir}/update.json

Copy-Item README.md  ${releaseDir}/README.md -force
Get-Content changelog.txt >> ${releaseDir}/README.md
Copy-Item changelog.txt ${releaseDir}/changelog.txt -force
Copy-Item changelogFull.txt ${releaseDir}/changelogFull.txt -force

log "All done"
