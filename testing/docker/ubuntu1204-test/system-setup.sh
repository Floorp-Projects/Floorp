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
apt_packages+=('libgl1-mesa-dri')
apt_packages+=('libgl1-mesa-dri:i386')
apt_packages+=('libgl1-mesa-glx')
apt_packages+=('libgl1-mesa-glx:i386')
apt_packages+=('libglapi-mesa')
apt_packages+=('libglapi-mesa:i386')
apt_packages+=('libglu1-mesa')
apt_packages+=('libglu1-mesa:i386')
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
apt_packages+=('mesa-common-dev')
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
apt_packages+=('unzip')
apt_packages+=('uuid')
apt_packages+=('vim')
apt_packages+=('wget')
apt_packages+=('xvfb')
apt_packages+=('yasm')
apt_packages+=('zip')

# Bug 1176031: need `xset` to disable screensavers
apt_packages+=('x11-xserver-utils')

# use Ubuntu's Python-2.7 (2.7.3 on Precise)
apt_packages+=('python-dev')
apt_packages+=('python-pip')

apt-get update
apt-get install -y --force-yes ${apt_packages[@]}

# set up tooltool (temporarily)
curl https://raw.githubusercontent.com/mozilla/build-tooltool/master/tooltool.py > /setup/tooltool.py
tooltool_fetch() {
    cat >manifest.tt
    python /setup/tooltool.py fetch
    rm manifest.tt
}

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

# sha256: tQ9peOfTn-DLKY-j-j6c5B0jVnIdFV5SiPnFfl8T6ac
mercurial==3.5
EOF
peep install -r requirements.txt

# Install node
tooltool_fetch <<'EOF'
[
{
    "size": 5676610, 
    "digest": "ce27b788dfd141a5ba7674332825fc136fe2c4f49a319dd19b3a87c8fffa7a97d86cbb8535661c9a68c9122719aa969fc6a8c886458a0df9fc822eec99ed130b", 
    "algorithm": "sha512", 
    "filename": "node-v0.10.36-linux-x64.tar.gz"
}
]

EOF
tar -C /usr/local -xz --strip-components 1 < node-*.tar.gz
node -v  # verify

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
cp /etc/apt/sources.list sources.list.orig
echo "deb file://$PWD/xcb precise all" >> /etc/apt/sources.list
apt-get update
apt-get -q -y --force-yes install libxcb1 libxcb-render0 libxcb-shm0 libxcb-glx0 libxcb-glx0:i386
libxcb1_version=$(dpkg-query -s libxcb1 | grep ^Version | awk '{ print $2 }')
[ "$libxcb1_version" = "1.8.1-2ubuntu2.1mozilla1" ] || exit 1
cp sources.list.orig /etc/apt/sources.list
apt-get update

# clean up

cd /
rm -rf /setup ~/.ccache ~/.cache ~/.npm
apt-get clean
apt-get autoclean
rm $0
