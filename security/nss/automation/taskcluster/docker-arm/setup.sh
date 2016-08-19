#!/usr/bin/env bash

set -v -e -x

export DEBIAN_FRONTEND=noninteractive

# Update.
apt-get -y update
apt-get -y dist-upgrade

apt_packages=()
apt_packages+=('build-essential')
apt_packages+=('ca-certificates')
apt_packages+=('curl')
apt_packages+=('python-dev')
apt_packages+=('python-pip')
apt_packages+=('python-setuptools')
apt_packages+=('zlib1g-dev')

# Install packages.
apt-get install -y --no-install-recommends ${apt_packages[@]}

# Latest Mercurial.
pip install --upgrade pip
pip install Mercurial

# Compiler options.
update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-5 30
update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-5 30

locale-gen en_US.UTF-8
dpkg-reconfigure locales

# Cleanup.
rm -rf ~/.ccache ~/.cache
apt-get autoremove -y
apt-get clean
apt-get autoclean
rm $0
