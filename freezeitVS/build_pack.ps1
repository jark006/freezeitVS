$ErrorActionPreference = 'Stop'

function log($logContent){
    Write-Host ("[{0}] {1}" -f (Get-Date), $logContent)
}

function abort($logContent){
    Write-Host ("[{0}] {1}" -f (Get-Date), $logContent) -ForegroundColor:Red 
    exit -1
}

$moduleProp = Get-Content magisk/module.prop
$id = ($moduleProp | Where-Object { $_ -match "id=" }).split('=')[1]
$version = ($moduleProp | Where-Object { $_ -match "version=" }).split('=')[1]
$versionCode = ($moduleProp | Where-Object { $_ -match "versionCode=" }).split('=')[1]
$zipFile = "${id}_v${version}.zip"

$releaseDir = "D:/Project-github/freezeitRelease"
$ndkPath = "D:/AndroidSDK/ndk/26.1.10909125"

$clang = "${ndkPath}/toolchains/llvm/prebuilt/windows-x86_64/bin/clang++.exe"
$sysroot = "--sysroot=${ndkPath}/toolchains/llvm/prebuilt/windows-x86_64/sysroot"
$cppFlags = "-std=c++20 -static -s -Ofast -Wall -Wextra -Wshadow -fno-exceptions -fno-rtti -DNDEBUG -fPIE"


log "Compiler... ARM64"
$target = "--target=aarch64-none-linux-android31"
& $clang $target $sysroot $cppFlags.Split(' ') -Iinclude src/main.cpp -o magisk/${id}ARM64
if (-not$?)
{
    abort "Compiler ARM64 fail"
}

log "Compiler... X64"
$target = "--target=x86_64-none-linux-android31"
& $clang $target $sysroot $cppFlags.Split(' ') -Iinclude src/main.cpp -o magisk/${id}X64
if (-not$?)
{
    abort "Compiler X64 fail"
}

Copy-Item changelog.txt magisk -force


if ((Test-Path $releaseDir) -ne "True")
{
    abort "None Path: $releaseDir"
}

log "Packing...  $zipFile"
$zipPath="${releaseDir}/${zipFile}"
if((Test-Path $zipPath) -eq "True"){
    log "Delete old file: $zipPath"
    del $zipPath
}
& ./7za.exe a "${zipPath}" ./magisk/* | Out-Null
if (-not$?)
{
    abort "Pack fail"
}

# https://github.com/jark006/freezeitRelease/raw/main/$zipFile
# https://github.com/jark006/freezeitRelease/raw/main/changelog.txt

# https://raw.githubusercontent.com/jark006/freezeitRelease/main/$zipFile
# https://raw.githubusercontent.com/jark006/freezeitRelease/main/changelog.txt

# https://raw.fastgit.org/jark006/freezeitRelease/main/$zipFile
# https://raw.fastgit.org/jark006/freezeitRelease/main/changelog.txt

# https://cdn.jsdelivr.net/gh/jark006/freezeitRelease/$zipFile
# https://cdn.jsdelivr.net/gh/jark006/freezeitRelease/changelog.txt

# https://gitee.com/jark006/freezeit-release/releases/download/${version}/$zipFile

# https://ghproxy.com/https://raw.githubusercontent.com/jark006/freezeitRelease/main/$zipFile
# https://ghproxy.com/https://raw.githubusercontent.com/jark006/freezeitRelease/main/changelog.txt

# https://external.githubfast.com/https/raw.githubusercontent.com/jark006/freezeitRelease/main/$zipFile
# https://external.githubfast.com/https/raw.githubusercontent.com/jark006/freezeitRelease/main/changelog.txt

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
