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
apt_packages+=('locales')
apt_packages+=('python-dev')
apt_packages+=('python-pip')
apt_packages+=('python-setuptools')
apt_packages+=('zlib1g-dev')

# Install packages.
apt-get install -y --no-install-recommends ${apt_packages[@]}

# Latest Mercurial.
pip install --upgrade pip
pip install Mercurial

locale-gen en_US.UTF-8
dpkg-reconfigure locales

# Cleanup.
rm -rf ~/.ccache ~/.cache
apt-get autoremove -y
apt-get clean
apt-get autoclean
rm $0
