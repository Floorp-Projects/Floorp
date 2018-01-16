#!/usr/bin/env bash

set -v -e -x

# Update packages.
export DEBIAN_FRONTEND=noninteractive
apt-get -y update && apt-get -y upgrade

# Install packages.
apt_packages=()
apt_packages+=('ca-certificates')
apt_packages+=('curl')
apt_packages+=('xz-utils')
apt_packages+=('mercurial')
apt_packages+=('git')
apt_packages+=('locales')
apt-get install -y --no-install-recommends ${apt_packages[@]}

# Download clang.
curl -L https://releases.llvm.org/3.9.1/clang+llvm-3.9.1-x86_64-linux-gnu-ubuntu-16.04.tar.xz -o clang.tar.xz
curl -L https://releases.llvm.org/3.9.1/clang+llvm-3.9.1-x86_64-linux-gnu-ubuntu-16.04.tar.xz.sig -o clang.tar.xz.sig
# Verify the signature.
gpg --keyserver pool.sks-keyservers.net --recv-keys B6C8F98282B944E3B0D5C2530FC3042E345AD05D
gpg --verify clang.tar.xz.sig
# Install into /usr/local/.
tar xJvf *.tar.xz -C /usr/local --strip-components=1

# Cleanup.
function cleanup() {
  rm -f clang.tar.xz clang.tar.xz.sig
}
trap cleanup ERR EXIT

locale-gen en_US.UTF-8
dpkg-reconfigure locales

# Cleanup.
rm -rf ~/.ccache ~/.cache
apt-get autoremove -y
apt-get clean
apt-get autoclean

# We're done. Remove this script.
rm $0
