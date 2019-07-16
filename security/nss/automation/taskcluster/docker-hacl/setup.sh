#!/usr/bin/env bash

set -v -e -x

# Update packages.
export DEBIAN_FRONTEND=noninteractive
apt-get -qq update
apt-get install --yes libssl-dev libsqlite3-dev g++-5 gcc-5 m4 make opam pkg-config python libgmp3-dev cmake curl libtool-bin autoconf wget locales
update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-5 200
update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-5 200

# Get clang-format-3.9
curl -LO https://releases.llvm.org/3.9.1/clang+llvm-3.9.1-x86_64-linux-gnu-ubuntu-16.04.tar.xz
curl -LO https://releases.llvm.org/3.9.1/clang+llvm-3.9.1-x86_64-linux-gnu-ubuntu-16.04.tar.xz.sig

# Verify the signature. The key used for verification was fetched via:
#    gpg --keyserver pgp.key-server.io --recv-keys B6C8F98282B944E3B0D5C2530FC3042E345AD05D
# Use a local copy to workaround bug 1565013.
gpg --no-default-keyring --keyring tmp.keyring --import /tmp/B6C8F98282B944E3B0D5C2530FC3042E345AD05D.asc
gpg --no-default-keyring --keyring tmp.keyring --verify clang+llvm-3.9.1-x86_64-linux-gnu-ubuntu-16.04.tar.xz.sig

# Install into /usr/local/.
tar xJvf *.tar.xz -C /usr/local --strip-components=1
# Cleanup.
rm *.tar.xz*

locale-gen en_US.UTF-8
dpkg-reconfigure locales

# Cleanup.
rm -rf ~/.ccache ~/.cache
apt-get autoremove -y
apt-get clean
apt-get autoclean
