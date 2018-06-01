#!/usr/bin/env bash
# This allows ubuntu-desktop to be installed without human interaction
export DEBIAN_FRONTEND=noninteractive

set -ve

test "$(whoami)" == 'root'

mkdir -p /setup
cd /setup

apt_packages=()
apt_packages+=('curl')
apt_packages+=('locales')
apt_packages+=('git')
apt_packages+=('python')
apt_packages+=('python-pip')
apt_packages+=('python3')
apt_packages+=('python3-pip')
apt_packages+=('shellcheck')
apt_packages+=('sudo')
apt_packages+=('wget')
apt_packages+=('xz-utils')

apt-get update
apt-get install -y "${apt_packages[@]}"

# Without this we get spurious "LC_ALL: cannot change locale (en_US.UTF-8)" errors,
# and python scripts raise UnicodeEncodeError when trying to print unicode characters.
locale-gen en_US.UTF-8
dpkg-reconfigure locales

su -c 'git config --global user.email "worker@mozilla.test"' worker
su -c 'git config --global user.name "worker"' worker

tooltool_fetch() {
    cat >manifest.tt
    /build/tooltool.py fetch
    rm manifest.tt
}

cd /build
# shellcheck disable=SC1091
. install-mercurial.sh

cd /
rm -rf /setup
