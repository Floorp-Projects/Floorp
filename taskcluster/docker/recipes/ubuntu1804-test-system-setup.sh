#!/usr/bin/env bash

set -ve

test "$(whoami)" == 'root'

mkdir -p /setup
cd /setup

# Enable i386 packages
dpkg --add-architecture i386

# To speed up docker image build times as well as number of network/disk I/O
# build a list of packages to be installed nad call it in one go.
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
apt_packages+=('gnome-icon-theme')
apt_packages+=('gstreamer1.0-plugins-base')
apt_packages+=('gstreamer1.0-plugins-good')
apt_packages+=('gstreamer1.0-tools')
apt_packages+=('language-pack-en-base')
apt_packages+=('libc6-dbg')
apt_packages+=('libasound2-dev')
apt_packages+=('libcanberra-gtk3-module')
apt_packages+=('libcanberra-pulse')
apt_packages+=('libcurl4-openssl-dev')
apt_packages+=('libdbus-1-dev')
apt_packages+=('libdbus-glib-1-dev')
apt_packages+=('libfreetype6')
apt_packages+=('libgconf2-dev')
apt_packages+=('libgl1-mesa-dri')
apt_packages+=('libgl1-mesa-glx')
apt_packages+=('libgstreamer-plugins-base1.0-dev')
apt_packages+=('libgstreamer1.0-dev')
apt_packages+=('libgtk2.0-dev')
apt_packages+=('libgtk-3-0')
apt_packages+=('libiw-dev')
apt_packages+=('libxcb1')
apt_packages+=('libxcb-render0')
apt_packages+=('libxcb-shm0')
apt_packages+=('libxcb-glx0')
apt_packages+=('libxcb-shape0')
apt_packages+=('libnotify-dev')
apt_packages+=('libpulse-dev')
apt_packages+=('libxt-dev')
apt_packages+=('libxxf86vm1')
apt_packages+=('llvm')
apt_packages+=('llvm-dev')
apt_packages+=('llvm-runtime')
apt_packages+=('mesa-common-dev')
apt_packages+=('net-tools')
apt_packages+=('pulseaudio')
apt_packages+=('pulseaudio-module-bluetooth')
apt_packages+=('pulseaudio-module-gconf')
apt_packages+=('python-dev')
apt_packages+=('python-pip')
apt_packages+=('qemu-kvm')
apt_packages+=('rlwrap')
apt_packages+=('screen')
apt_packages+=('software-properties-common')
apt_packages+=('sudo')
apt_packages+=('ttf-dejavu')
apt_packages+=('ubuntu-desktop')
apt_packages+=('unzip')
apt_packages+=('uuid')
apt_packages+=('wget')
apt_packages+=('xvfb')
apt_packages+=('yasm')
apt_packages+=('zip')

# Make sure we have libraries for 32-bit tests
apt_packages+=('fontconfig:i386')
apt_packages+=('libxt6:i386')
apt_packages+=('libpulse0:i386')
apt_packages+=('libxtst6:i386')
apt_packages+=('libsecret-1-0:i386')
apt_packages+=('libavcodec-extra57:i386')
apt_packages+=('libgtk2.0-0:i386')
apt_packages+=('libgtk-3-0:i386')
apt_packages+=('libdbus-glib-1-2:i386')

# xvinfo for test-linux.sh to monitor Xvfb startup
apt_packages+=('x11-utils')

# Bug 1232407 - this allows the user to start vnc
apt_packages+=('x11vnc')

# Bug 1176031 - need `xset` to disable screensavers
apt_packages+=('x11-xserver-utils')

# Build a list of packages to install from the multiverse repo.
apt_packages+=('ubuntu-restricted-extras')

# APT update takes very long on Ubuntu. Run it at the last possible minute.
apt-get update

# This allows ubuntu-desktop to be installed without human interaction.
# Also force the cleanup after installation of packages to reduce image size.
export DEBIAN_FRONTEND=noninteractive
apt-get install -y -f "${apt_packages[@]}" && rm -rf /var/lib/apt/lists/*

# Install tooltool, mercurial and node now that dependencies are in place.
. /setup/common.sh
. /setup/install-mercurial.sh
. /setup/install-node.sh

# Upgrade pip and install virtualenv to specified versions.
pip install --upgrade pip==19.2.3
hash -r
pip install virtualenv==15.2.0

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

# Build a list of packages to purge from the image.
apt_packages=()
apt_packages+=('*alsa*')
apt_packages+=('git')
apt_packages+=('gnome-calendar')
apt_packages+=('gnome-initial-setup')
apt_packages+=('gnome-mahjongg')
apt_packages+=('gnome-mines')
apt_packages+=('gnome-sudoku')
apt_packages+=('ubuntu-release-upgrader*')
apt_packages+=('update-manager-core')
apt_packages+=('update-manager')
apt_packages+=('*whoopsie*')
apt_packages+=('yelp')

# Purge unnecessary packages
apt-get purge -y -f "${apt_packages[@]}"

# Clear apt cache one last time
rm -rf /var/cache/apt/archives

# We don't need no docs!
rm -rf /usr/share/help /usr/share/doc /usr/share/man

# Remove all locale files other than en_US.UTF-8
rm -rf /usr/share/locale/   /usr/share/locale-langpack/     /usr/share/locales/
echo "en_US.UTF-8 UTF-8" > /var/lib/locales/supported.d/en
locale-gen

# Further cleanup
cd /
rm -rf /setup ~/.ccache ~/.cache ~/.npm
apt-get -y autoremove
apt-get clean
apt-get autoclean
rm -f "$0"
