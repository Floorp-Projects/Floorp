#!/usr/bin/env bash

set -ve

test "$(whoami)" == 'root'

mkdir -p /setup
cd /setup

# enable i386 packages
dpkg --add-architecture i386

apt_packages=()

apt_packages+=('autoconf2.13')
apt_packages+=('bluez-cups')
apt_packages+=('build-essential')
apt_packages+=('ca-certificates')
apt_packages+=('ccache')
apt_packages+=('compiz')
apt_packages+=('compizconfig-settings-manager')
apt_packages+=('curl')
apt_packages+=('dbus')
apt_packages+=('dbus-x11')
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
apt_packages+=('libgtk2.0-dev')
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
apt_packages+=('nano')
apt_packages+=('net-tools')
apt_packages+=('pulseaudio')
apt_packages+=('pulseaudio-utils')
apt_packages+=('qemu-kvm')
apt_packages+=('rlwrap')
apt_packages+=('screen')
apt_packages+=('software-properties-common')
apt_packages+=('sudo')
apt_packages+=('tar')
apt_packages+=('task-gnome-desktop')
apt_packages+=('ttf-dejavu')
apt_packages+=('unzip')
apt_packages+=('uuid')
apt_packages+=('vim')
apt_packages+=('wget')
apt_packages+=('xvfb')
apt_packages+=('x11-common')
apt_packages+=('yasm')
apt_packages+=('zip')
apt_packages+=('libsecret-1-0:i386')

# Make sure we have X libraries for 32-bit tests
apt_packages+=('libxt6:i386')
apt_packages+=('libpulse0:i386')
apt_packages+=('libasound2:i386')
apt_packages+=('libxtst6:i386')
apt_packages+=('libgtk2.0-0:i386')

# get xvinfo for test-linux.sh to monitor Xvfb startup
apt_packages+=('x11-utils')

# Bug 1232407 - this allows the user to start vnc
apt_packages+=('x11vnc')

# Bug 1176031: need `xset` to disable screensavers
apt_packages+=('x11-xserver-utils')

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

# first upgrade debian:buster built-in pip, then install older pip
pip install --upgrade pip
pip install pip==9.0.3
hash -r
pip install virtualenv==15.2.0

. /setup/install-node.sh

# Install Valgrind (trunk, late Jan 2016) and do some crude sanity
# checks.  It has to go in /usr/local, otherwise it won't work.  Copy
# the launcher binary to /usr/bin, though, so that direct invokations
# of /usr/bin/valgrind also work.  Also install libc6-dbg since
# Valgrind won't work at all without the debug symbols for libc.so and
# ld.so being available.
tooltool_fetch <<'EOF'
[
{
    "size": 41331092,
    "visibility": "public",
    "digest": "a89393c39171b8304fc262094a650df9a756543ffe9fbec935911e7b86842c4828b9b831698f97612abb0eca95cf7f7b3ff33ea7a9b0313b30c9be413a5efffc",
    "algorithm": "sha512",
    "filename": "valgrind-15775-3206-ubuntu1204.tgz"
}
]
EOF
cp valgrind-15775-3206-ubuntu1204.tgz /tmp
(cd / && tar xzf /tmp/valgrind-15775-3206-ubuntu1204.tgz)
rm /tmp/valgrind-15775-3206-ubuntu1204.tgz
cp /usr/local/bin/valgrind /usr/bin/valgrind
apt-get install -y libc6-dbg
valgrind --version
valgrind date

# Until bug 1511527 is fixed, remove the file from the image to ensure it's not there.
rm -f /usr/local/bin/linux64-minidump_stackwalk

# install gstreamer0.10 from jessie
cp /etc/apt/sources.list sources.list.bak
echo 'deb http://ftp.de.debian.org/debian jessie main' >> /etc/apt/sources.list
apt-get update
apt-get install -f
apt-get install -y -f -q        \
    libtag1-vanilla             \
    libtag1c2a                  \
    libsidplay1

apt-get install -y -f -q        \
    gstreamer0.10-plugins-base  \
    gstreamer0.10-plugins-good  \
    gstreamer0.10-pulseaudio    \
    gstreamer0.10-tools         \
    gstreamer0.10-tools         \
    libgstreamer-plugins-base0.10-dev \
    libgstreamer0.10-dev        \
    gstreamer0.10-plugins-ugly

# TEMPORARY: we do not want flash installed, but the above pulls it in (bug 1349208)
rm -f /usr/lib/flashplugin-installer/libflashplayer.so

apt-get -q -y -f install        \
    libxcb1                     \
    libxcb-render0              \
    libxcb-shm0                 \
    libxcb-glx0                 \
    libxcb-shape0

apt-get -q -y -f install        \
    libgl1-mesa-dri             \
    libgl1-mesa-glx             \
    mesa-common-dev

# additional packages for linux32 tests
# including fc-cache:i386 to pre-build the font cache for i386 binaries
apt-get update
apt-get -q -y -f install        \
    libavcodec-extra58:i386     \
    libgtk-3-0:i386             \
    libdbus-glib-1-2:i386       \
    fontconfig:i386

# libavcodec-extra58 uninstalls pulseaudio-module-gsettings
# reinstall it now.
apt-get -q -y -f install        \
    pulseaudio-module-gsettings

# revert the addition of jessie repos
cp sources.list.bak /etc/apt/sources.list

# clean up
apt-get -y autoremove

# We don't need no docs!
rm -rf /usr/share/help /usr/share/doc /usr/share/man

cd /
rm -rf /setup ~/.ccache ~/.cache ~/.npm
apt-get clean
apt-get autoclean
rm -f "$0"
