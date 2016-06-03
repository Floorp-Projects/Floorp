#!/usr/bin/env bash

set -v -e -x

apt_packages=()
apt_packages+=('build-essential')
apt_packages+=('ca-certificates')
apt_packages+=('curl')
apt_packages+=('mercurial')
apt_packages+=('npm')
apt_packages+=('git')
apt_packages+=('valgrind')
apt_packages+=('zlib1g-dev')

# 32-bit builds
apt_packages+=('lib32z1-dev')
apt_packages+=('gcc-multilib')
apt_packages+=('g++-multilib')

# Install prerequisites.
apt-get -y update
export DEBIAN_FRONTEND=noninteractive
apt-get install -y --no-install-recommends curl apt-utils

# clang(-format)-3.8
apt_packages+=('clang-3.8')
apt_packages+=('clang-format-3.8')
curl http://llvm.org/apt/llvm-snapshot.gpg.key | apt-key add -
echo "deb http://llvm.org/apt/xenial/ llvm-toolchain-xenial-3.8 main" > /etc/apt/sources.list.d/docker.list

# Install the first round of packages.
apt-get -y update
apt-get install -y --no-install-recommends ${apt_packages[@]}

# gcc 6
apt_packages=()
apt_packages+=('g++-6')
apt_packages+=('g++-4.8')
apt_packages+=('g++-6-multilib')
apt_packages+=('g++-4.8-multilib')
apt-key adv --keyserver keyserver.ubuntu.com --recv-keys 60C317803A41BA51845E371A1E9377A2BA9EF27F
echo "deb http://ppa.launchpad.net/ubuntu-toolchain-r/test/ubuntu xenial main" > /etc/apt/sources.list.d/toolchain.list

# Install the second round of packages.
apt-get -y update
apt-get install -y --no-install-recommends ${apt_packages[@]}

# 32-bit builds
ln -s /usr/include/x86_64-linux-gnu/zconf.h /usr/include

# Compiler options.
update-alternatives --install /usr/bin/gcc gcc /usr/bin/clang-3.8 5
update-alternatives --install /usr/bin/g++ g++ /usr/bin/clang++-3.8 5
update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-4.8 10
update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-4.8 10
update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-6 20
update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-6 20
update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-5 30
update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-5 30

locale-gen en_US.UTF-8
dpkg-reconfigure locales

# Install required Node modules.
su -c "npm install flatmap js-yaml merge slugid" worker

# Cleanup.
rm -rf ~/.ccache ~/.cache
apt-get clean
apt-get autoclean
rm $0
