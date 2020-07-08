#!/usr/bin/env bash

set -ve

test "$(whoami)" == 'root'

mkdir -p /setup
cd /setup

# To speed up docker image build times as well as number of network/disk I/O
# build a list of packages to be installed nad call it in one go.
apt_packages=()

# Core utilities
apt_packages+=('curl')
apt_packages+=('git')
apt_packages+=('libdbus-glib-1-2')
apt_packages+=('libgtk-3-0')
apt_packages+=('libx11-6')
apt_packages+=('libxcb1')
apt_packages+=('libxcursor1')
apt_packages+=('python-dev-is-python2')
apt_packages+=('python2')
apt_packages+=('python3-pip')
apt_packages+=('qemu-kvm')
apt_packages+=('screen')
apt_packages+=('sudo')
apt_packages+=('unzip')
apt_packages+=('uuid')
apt_packages+=('wget')
apt_packages+=('x11-utils')
apt_packages+=('xvfb')
apt_packages+=('xwit')
apt_packages+=('yasm')
apt_packages+=('zip')


# APT update takes very long on Ubuntu. Run it at the last possible minute.
apt-get update

# This allows ubuntu-desktop to be installed without human interaction.
# Also force the cleanup after installation of packages to reduce image size.
export DEBIAN_FRONTEND=noninteractive
apt-get install -y -f "${apt_packages[@]}"

# Clean up installation files.
rm -rf /var/lib/apt/lists/*

# Ubuntu 20.04 officially deprecates Python 2, so this is the workaround.
curl https://bootstrap.pypa.io/get-pip.py --output get-pip.py
python get-pip.py

# Install tooltool and mercurial now that dependencies are in place.
. /setup/common.sh
. /setup/install-mercurial.sh

# Install specified versions of virtualenv and zstandard.
pip3 install virtualenv==15.2.0
pip3 install zstandard==0.13.0

# Clear apt cache one last time
rm -rf /var/cache/apt/archives

# We don't need no docs!
rm -rf /usr/share/help /usr/share/doc /usr/share/man

# Remove all locale files other than en_US.UTF-8
rm -rf /usr/share/locale/   /usr/share/locale-langpack/     /usr/share/locales/

# Further cleanup
cd /
rm -rf /setup ~/.ccache ~/.cache ~/.npm
apt-get -y autoremove
apt-get clean
apt-get autoclean
rm -f "$0"