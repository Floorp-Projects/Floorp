#!/usr/bin/env bash

set -ve

test "$(whoami)" == 'root'

mkdir -p /setup
cd /setup

apt_packages=()

apt_packages+=('autoconf2.13')
apt_packages+=('bluez-cups')
apt_packages+=('build-essential')
apt_packages+=('ca-certificates')
apt_packages+=('ccache')
apt_packages+=('curl')
apt_packages+=('fonts-kacst')
apt_packages+=('fonts-kacst-one')
apt_packages+=('fonts-liberation')
apt_packages+=('fonts-stix')
apt_packages+=('fonts-unfonts-core')
apt_packages+=('fonts-unfonts-extra')
apt_packages+=('fonts-vlgothic')
apt_packages+=('g++-multilib')
apt_packages+=('gcc-multilib')
apt_packages+=('gir1.2-gnomebluetooth-1.0')
apt_packages+=('git')
apt_packages+=('gnome-keyring')
apt_packages+=('libasound2-dev')
apt_packages+=('libcanberra-pulse')
apt_packages+=('libcurl4-openssl-dev')
apt_packages+=('libdbus-1-dev')
apt_packages+=('libdbus-glib-1-dev')
apt_packages+=('libgconf2-dev')
apt_packages+=('libiw-dev')
apt_packages+=('libnotify-dev')
apt_packages+=('libpulse-dev')
apt_packages+=('libsox-fmt-alsa')
apt_packages+=('libxt-dev')
apt_packages+=('libxxf86vm1')
apt_packages+=('llvm')
apt_packages+=('llvm-dev')
apt_packages+=('llvm-runtime')
apt_packages+=('locales')
apt_packages+=('locales-all')
apt_packages+=('net-tools')
apt_packages+=('qemu-kvm')
apt_packages+=('rlwrap')
apt_packages+=('screen')
apt_packages+=('software-properties-common')
apt_packages+=('sudo')
apt_packages+=('tar')
apt_packages+=('ttf-dejavu')
apt_packages+=('unzip')
apt_packages+=('uuid')
apt_packages+=('wget')
apt_packages+=('xvfb')
apt_packages+=('zip')

# use Ubuntu's Python-2.7 (2.7.3 on Precise)
apt_packages+=('python-dev')
apt_packages+=('python-pip')

apt-get update
# This allows packages to be installed without human interaction
export DEBIAN_FRONTEND=noninteractive
apt-get install -y -f "${apt_packages[@]}"

dpkg-reconfigure locales

. /setup/common.sh
. /setup/install-mercurial.sh

# pip 19.3 is causing errors building the docker image, pin to 19.2.3 for now.
# See https://github.com/pypa/pip/issues/7206
pip install --upgrade pip==19.2.3
hash -r
pip install virtualenv==15.2.0

# clean up
apt-get -y autoremove

# We don't need no docs!
rm -rf /usr/share/help /usr/share/doc /usr/share/man

cd /
rm -rf /setup ~/.ccache ~/.cache ~/.npm
apt-get clean
apt-get autoclean
rm -f "$0"
