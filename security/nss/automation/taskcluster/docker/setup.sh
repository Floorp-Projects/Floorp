#!/usr/bin/env bash

set -v -e -x

# Update packages.
export DEBIAN_FRONTEND=noninteractive
apt-get -y update && apt-get -y upgrade

# Need this to add keys for PPAs below.
apt-get install -y --no-install-recommends apt-utils

apt_packages=()
apt_packages+=('build-essential')
apt_packages+=('ca-certificates')
apt_packages+=('clang-5.0')
apt_packages+=('curl')
apt_packages+=('npm')
apt_packages+=('git')
apt_packages+=('golang-1.6')
apt_packages+=('libxml2-utils')
apt_packages+=('locales')
apt_packages+=('ninja-build')
apt_packages+=('pkg-config')
apt_packages+=('zlib1g-dev')
apt_packages+=('cmake')

# 32-bit builds
apt_packages+=('lib32z1-dev')
apt_packages+=('gcc-multilib')
apt_packages+=('g++-multilib')

# ct-verif and sanitizers
apt_packages+=('valgrind')

# Latest Mercurial.
apt_packages+=('mercurial')
apt-key adv --keyserver keyserver.ubuntu.com --recv-keys 41BD8711B1F0EC2B0D85B91CF59CE3A8323293EE
echo "deb http://ppa.launchpad.net/mercurial-ppa/releases/ubuntu xenial main" > /etc/apt/sources.list.d/mercurial.list

# gcc 4.8 and 6
apt_packages+=('g++-6')
apt_packages+=('g++-4.8')
apt_packages+=('g++-6-multilib')
apt_packages+=('g++-4.8-multilib')
apt-key adv --keyserver keyserver.ubuntu.com --recv-keys 60C317803A41BA51845E371A1E9377A2BA9EF27F
echo "deb http://ppa.launchpad.net/ubuntu-toolchain-r/test/ubuntu xenial main" > /etc/apt/sources.list.d/toolchain.list

# Install packages.
apt-get -y update
apt-get install -y --no-install-recommends ${apt_packages[@]}

# Latest version of abigail-tools
apt-get install -y libxml2-dev autoconf libelf-dev libdw-dev libtool
git clone git://sourceware.org/git/libabigail.git
cd ./libabigail
autoreconf -fi
./configure --prefix=/usr --disable-static --disable-apidoc --disable-manual
make
make install
cd ..
apt-get remove -y libxml2-dev autoconf libtool
rm -rf libabigail

# Install latest Rust (stable).
su worker -c "curl https://sh.rustup.rs -sSf | sh -s -- -y"

locale-gen en_US.UTF-8
dpkg-reconfigure locales

# Cleanup.
rm -rf ~/.ccache ~/.cache
apt-get autoremove -y
apt-get clean
apt-get autoclean
rm $0
