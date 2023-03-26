
# Termux 安装 sshd
```sh
#!/bin/bash
set -e
pkg install -y openssh git make cmake vim git wget curl zip unzip man

cat >> ubuntu.sh <<EOF
#!/bin/sh
uesr=`whoami`
ip=`ifconfig 2>>/dev/null | grep 192.168 | cut -d ' ' -f 10`
echo ssh $uesr@$ip -p 8022
sshd
proot-distro login ubuntu --bind "/sdcard" --bind "/data/data/com.termux/files/home:/root"
EOF

chmod +x ubuntu.sh

nano /etc/profile
alias lt='ls --human-readable --size -1 -S --classify'
alias ll='ls -al'

```

---
# Termux 安装完整Linux发行版 [WIKI链接](https://wiki.termux.com/wiki/PRoot#Installing_Linux_distributions)


## 安装发行版管理工具

```sh
pkg install proot-distro

proot-distro list - show the supported distributions and their status.
proot-distro install - install a distribution.
proot-distro login - start a root shell for the distribution.
proot-distro remove - uninstall the distribution.
proot-distro reset - reinstall the distribution.

proot-distro install ubuntu # 开启魔法网速才快

# 登陆 提前给予全部存储权限 或运行 termux-setup-storage 进行授权
# https://wiki.termux.com/wiki/Internal_and_external_storage

proot-distro login ubuntu --bind "/sdcard" --bind "/data/data/com.termux/files/home:/root"

```

## 安装 Ubuntu 脚本
```sh
#!/bin/bash
# 遇到错误立即终止
set -e
pkg install proot-distro
proot-distro install ubuntu
proot-distro login ubuntu
```

## 进入 Ubuntu 设置源
```
mv /etc/apt/sources.list /etc/apt/sources.listbk
nano /etc/apt/sources.list

# 官方 ARM64 源， 貌似其他源不支持 arm64
deb [arch=arm64] http://ports.ubuntu.com/ jammy main multiverse universe
deb [arch=arm64] http://ports.ubuntu.com/ jammy-security main multiverse universe
deb [arch=arm64] http://ports.ubuntu.com/ jammy-backports main multiverse universe
deb [arch=arm64] http://ports.ubuntu.com/ jammy-updates main multiverse universe

# 若警告 Certificate verification failed
# 可将 source.list https 改 http
# 安装 apt install ca-certificates
# 可将 source.list http 改回 https
nano 清空 alt+a ctrl+end ctrl+k
nano /etc/apt/sources.list && apt install ca-certificates
ctrl+\

apt update && apt upgrade
apt install -y make cmake vim git wget curl zip unzip man
```


# 安装 glibc
```sh
apt install -y bison
wget http://ftp.gnu.org/pub/gnu/glibc/glibc-2.35.tar.bz2
tar -xf glibc*
cd glibc-2.35
mkdir build
cd build
../configure --prefix=/opt/glibc-2.35
make && make install
nano /etc/profile

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/opt/glibc-2.35/lib

source /etc/profile
```


https://coder.com/docs/code-server/latest/guide#ssh-into-code-server-on-vs-code

https://developers.cloudflare.com/cloudflare-one/connections/connect-apps/install-and-setup/installation#linux
wget https://github.com/cloudflare/cloudflared/releases/latest/download/cloudflared-linux-arm64

# 安装远程网页 vscode 
[code-server Github](https://github.com/coder/code-server/releases)
```
一键安装
curl -fsSL https://code-server.dev/install.sh | sh -s -- --dry-run

手动安装
如果出现 OpenSSL: error:0A000126 错误就一直重试，直至成功
wget https://github.com/coder/code-server/releases/download/v4.4.0/code-server_4.4.0_arm64.deb
dpkg -i code-server*.deb

配置
nano ~/.config/code-server/config.yaml

bind-addr: 127.0.0.1:8080
auth: password
password: 8435b314889e1170fc6d27fb
cert: false

bind-addr: 0.0.0.0:8080
auth: none
password: 123
cert: false

开始
code-server 

自定义
code-server --auth password --port 8080
```