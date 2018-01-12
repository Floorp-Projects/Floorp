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
apt_packages+=('curl')
apt_packages+=('npm')
apt_packages+=('git')
apt_packages+=('golang-1.6')
apt_packages+=('libxml2-utils')
apt_packages+=('locales')
apt_packages+=('ninja-build')
apt_packages+=('pkg-config')
apt_packages+=('zlib1g-dev')

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

# Download clang.
curl -LO https://releases.llvm.org/4.0.0/clang+llvm-4.0.0-x86_64-linux-gnu-ubuntu-16.04.tar.xz
curl -LO https://releases.llvm.org/4.0.0/clang+llvm-4.0.0-x86_64-linux-gnu-ubuntu-16.04.tar.xz.sig
# Verify the signature.
gpg --keyserver pool.sks-keyservers.net --recv-keys B6C8F98282B944E3B0D5C2530FC3042E345AD05D
gpg --verify *.tar.xz.sig
# Install into /usr/local/.
tar xJvf *.tar.xz -C /usr/local --strip-components=1
# Cleanup.
rm *.tar.xz*

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
