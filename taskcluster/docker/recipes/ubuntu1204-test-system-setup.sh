#!/usr/bin/env bash

set -ve

test `whoami` == 'root'

mkdir -p /setup
cd /setup

apt_packages=()

apt_packages+=('alsa-base')
apt_packages+=('alsa-utils')
apt_packages+=('autoconf2.13')
apt_packages+=('bluez-alsa')
apt_packages+=('bluez-alsa:i386')
apt_packages+=('bluez-cups')
apt_packages+=('bluez-gstreamer')
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
apt_packages+=('gstreamer0.10-alsa')
apt_packages+=('gstreamer0.10-ffmpeg')
apt_packages+=('gstreamer0.10-plugins-bad')
apt_packages+=('gstreamer0.10-plugins-base')
apt_packages+=('gstreamer0.10-plugins-good')
apt_packages+=('gstreamer0.10-plugins-ugly')
apt_packages+=('gstreamer0.10-tools')
apt_packages+=('language-pack-en-base')
apt_packages+=('libasound2-dev')
apt_packages+=('libasound2-plugins:i386')
apt_packages+=('libcanberra-pulse')
apt_packages+=('libcurl4-openssl-dev')
apt_packages+=('libdbus-1-dev')
apt_packages+=('libdbus-glib-1-dev')
apt_packages+=('libdrm-intel1:i386')
apt_packages+=('libdrm-nouveau1a:i386')
apt_packages+=('libdrm-radeon1:i386')
apt_packages+=('libdrm2:i386')
apt_packages+=('libexpat1:i386')
apt_packages+=('libgconf2-dev')
apt_packages+=('libgnome-bluetooth8')
apt_packages+=('libgstreamer-plugins-base0.10-dev')
apt_packages+=('libgstreamer0.10-dev')
apt_packages+=('libgtk2.0-dev')
apt_packages+=('libiw-dev')
apt_packages+=('libllvm2.9')
apt_packages+=('libllvm3.0:i386')
apt_packages+=('libncurses5:i386')
apt_packages+=('libnotify-dev')
apt_packages+=('libpulse-dev')
apt_packages+=('libpulse-mainloop-glib0:i386')
apt_packages+=('libpulsedsp:i386')
apt_packages+=('libsdl1.2debian:i386')
apt_packages+=('libsox-fmt-alsa')
apt_packages+=('libx11-xcb1:i386')
apt_packages+=('libxdamage1:i386')
apt_packages+=('libxfixes3:i386')
apt_packages+=('libxt-dev')
apt_packages+=('libxxf86vm1')
apt_packages+=('libxxf86vm1:i386')
apt_packages+=('llvm')
apt_packages+=('llvm-2.9')
apt_packages+=('llvm-2.9-dev')
apt_packages+=('llvm-2.9-runtime')
apt_packages+=('llvm-dev')
apt_packages+=('llvm-runtime')
apt_packages+=('nano')
apt_packages+=('pulseaudio')
apt_packages+=('pulseaudio-module-X11')
apt_packages+=('pulseaudio-module-bluetooth')
apt_packages+=('pulseaudio-module-gconf')
apt_packages+=('rlwrap')
apt_packages+=('screen')
apt_packages+=('software-properties-common')
apt_packages+=('sudo')
apt_packages+=('tar')
apt_packages+=('ttf-arphic-uming')
apt_packages+=('ttf-dejavu')
apt_packages+=('ttf-indic-fonts-core')
apt_packages+=('ttf-kannada-fonts')
apt_packages+=('ttf-oriya-fonts')
apt_packages+=('ttf-paktype')
apt_packages+=('ttf-punjabi-fonts')
apt_packages+=('ttf-sazanami-mincho')
apt_packages+=('ubuntu-desktop')
apt_packages+=('unzip')
apt_packages+=('uuid')
apt_packages+=('vim')
apt_packages+=('wget')
apt_packages+=('xvfb')
apt_packages+=('yasm')
apt_packages+=('zip')

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
# This allows ubuntu-desktop to be installed without human interaction
export DEBIAN_FRONTEND=noninteractive
apt-get install -y --force-yes ${apt_packages[@]}

dpkg-reconfigure locales

tooltool_fetch() {
    cat >manifest.tt
    python /setup/tooltool.py fetch
    rm manifest.tt
}

. /tmp/install-mercurial.sh

# install peep
tooltool_fetch <<'EOF'
[
{
    "size": 26912,
    "digest": "9d730ed7852d4d217aaddda959cd5f871ef1b26dd6c513a3780bbb04a5a93a49d6b78e95c2274451a1311c10cc0a72755b269dc9af62640474e6e73a1abec370",
    "algorithm": "sha512",
    "filename": "peep-2.4.1.tar.gz",
    "unpack": false
}
]
EOF
pip install peep-2.4.1.tar.gz

# remaining Python utilities are installed with `peep` from upstream
# repositories; peep verifies file integrity for us
cat >requirements.txt <<'EOF'
# wheel
# sha256: 90pZQ6kAXB6Je8-H9-ivfgDAb6l3e5rWkfafn6VKh9g
# tarball:
# sha256: qryO8YzdvYoqnH-SvEPi_qVLEUczDWXbkg7zzpgS49w
virtualenv==13.1.2
EOF
peep install -r requirements.txt

# Install node
wget https://nodejs.org/dist/v5.0.0/node-v5.0.0-linux-x64.tar.gz
echo 'ef73b59048a0ed11d01633f0061627b7a9879257deb9add2255e4d0808f8b671  node-v5.0.0-linux-x64.tar.gz' | sha256sum -c
tar -C /usr/local -xz --strip-components 1 < node-v5.0.0-linux-x64.tar.gz
node -v  # verify

# Install custom-built Debian packages.  These come from a set of repositories
# packaged in tarballs on tooltool to make them replicable.  Because they have
# inter-dependenices, we install all repositories first, then perform the
# installation.
cp /etc/apt/sources.list sources.list.orig

# Install a slightly newer version of libxcb
# See bug 975216 for the original build of these packages
# NOTE: if you're re-creating this, the tarball contains an `update.sh` which will rebuild the repository.
tooltool_fetch <<'EOF'
[
{
    "size": 9708011, 
    "digest": "45005c7e1fdb4839f3bb96bfdb5e448672e7e40fa3cfc22ef646e2996a541f151e88c61b8a568cc00b7fcf5cb5e98e00c4e603acb0b73f85125582fa00aae76e", 
    "algorithm": "sha512", 
    "filename": "xcb-repo-1.8.1-2ubuntu2.1mozilla1.tgz"
}
]
EOF
tar -zxf xcb-repo-*.tgz
echo "deb file://$PWD/xcb precise all" >> /etc/apt/sources.list

# Install a patched version of mesa, per bug 1227637.  Origin of the packages themselves is unknown, as
# these binaries were copied from the apt repositories used by puppet.  Ask rail for more information.
# NOTE: if you're re-creating this, the tarball contains an `update.sh` which will rebuild the repository.
tooltool_fetch <<'EOF'
[
{
    "size": 590643702, 
    "visibility": "public", 
    "digest": "f03b11987c218e57073d1b7eec6cc0a753d48f600df8dde0a35fa7c4d4d30b3891c9cbcaee38ade23f038e72951cb15f0dca3f7c76cbf5bad5526baf13e91929", 
    "algorithm": "sha512", 
    "filename": "mesa-repo-9.2.1-1ubuntu3~precise1mozilla2.tgz"
}
]
EOF
tar -zxf mesa-repo-*.tgz
echo "deb file://$PWD/mesa precise all" >> /etc/apt/sources.list

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

# Fetch the minidump_stackwalk binary specified by the in-tree tooltool manifest.
python /setup/tooltool.py fetch -m /tmp/minidump_stackwalk.manifest
rm /tmp/minidump_stackwalk.manifest
mv linux64-minidump_stackwalk /usr/local/bin/
chmod +x /usr/local/bin/linux64-minidump_stackwalk

apt-get update

apt-get -q -y --force-yes install \
    libxcb1 \
    libxcb-render0 \
    libxcb-shm0 \
    libxcb-glx0 \
    libxcb-shape0 libxcb-glx0:i386
libxcb1_version=$(dpkg-query -s libxcb1 | grep ^Version | awk '{ print $2 }')
[ "$libxcb1_version" = "1.8.1-2ubuntu2.1mozilla1" ] || exit 1

apt-get -q -y --force-yes install \
    libgl1-mesa-dev-lts-saucy:i386 \
    libgl1-mesa-dri-lts-saucy \
    libgl1-mesa-dri-lts-saucy:i386 \
    libgl1-mesa-glx-lts-saucy \
    libgl1-mesa-glx-lts-saucy:i386 \
    libglapi-mesa-lts-saucy \
    libglapi-mesa-lts-saucy:i386 \
    libxatracker1-lts-saucy \
    mesa-common-dev-lts-saucy:i386
mesa_version=$(dpkg-query -s libgl1-mesa-dri-lts-saucy | grep ^Version | awk '{ print $2 }')
[ "$mesa_version" = "9.2.1-1ubuntu3~precise1mozilla2" ] || exit 1

# additional packages for linux32 tests
apt-get -q -y --force-yes install \
    libcanberra-gtk3-module:i386 \
    libcanberra-gtk-module:i386 \
    libdbus-glib-1-2:i386 \
    libgtk-3-0:i386 \
    openjdk-7-jdk:i386

# revert the list of repos
cp sources.list.orig /etc/apt/sources.list
apt-get update

# node 5 requires a C++11 compiler.
add-apt-repository ppa:ubuntu-toolchain-r/test
apt-get update
apt-get -y install gcc-4.8 g++-4.8
update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-4.8 20 --slave /usr/bin/g++ g++ /usr/bin/g++-4.8
update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-4.6 10 --slave /usr/bin/g++ g++ /usr/bin/g++-4.6

# clean up
apt_packages+=('mesa-common-dev')

cd /
rm -rf /setup ~/.ccache ~/.cache ~/.npm
apt-get clean
apt-get autoclean
rm -f $0
