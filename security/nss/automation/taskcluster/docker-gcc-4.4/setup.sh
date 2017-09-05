#!/usr/bin/env bash

set -v -e -x

# Update packages.
export DEBIAN_FRONTEND=noninteractive
apt-get -y update && apt-get -y upgrade

apt_packages=()
apt_packages+=('ca-certificates')
apt_packages+=('g++-4.4')
apt_packages+=('gcc-4.4')
apt_packages+=('locales')
apt_packages+=('make')
apt_packages+=('mercurial')
apt_packages+=('zlib1g-dev')

# Install packages.
apt-get -y update
apt-get install -y --no-install-recommends ${apt_packages[@]}

locale-gen en_US.UTF-8
dpkg-reconfigure locales

# Cleanup.
rm -rf ~/.ccache ~/.cache
apt-get autoremove -y
apt-get clean
apt-get autoclean
rm $0
