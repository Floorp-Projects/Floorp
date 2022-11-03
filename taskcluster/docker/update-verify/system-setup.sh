#!/usr/bin/env bash

set -ve

test "$(whoami)" == 'root'

mkdir -p /setup
cd /setup

apt_packages=()
apt_packages+=('curl')
apt_packages+=('locales')
apt_packages+=('python3-pip')
apt_packages+=('python3-aiohttp')
apt_packages+=('shellcheck')
apt_packages+=('sudo')

apt-get update
apt-get install "${apt_packages[@]}"

# Without this we get spurious "LC_ALL: cannot change locale (en_US.UTF-8)" errors,
# and python scripts raise UnicodeEncodeError when trying to print unicode characters.
locale-gen en_US.UTF-8
dpkg-reconfigure locales

su -c 'git config --global user.email "worker@mozilla.test"' worker
su -c 'git config --global user.name "worker"' worker

rm -rf /setup
