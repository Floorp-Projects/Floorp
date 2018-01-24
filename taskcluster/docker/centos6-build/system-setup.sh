#!/usr/bin/env bash

set -ve

test "$(whoami)" == 'root'

# lots of goodies in EPEL
yum install -y epel-release

# this sometimes fails, so we repeat it
yum makecache || yum makecache

yum shell -y <<'EOF'
# This covers a bunch of requirements
groupinstall Base

install findutils
install gawk
install ppl
install cpp
install grep
install gzip
install sed
install tar
install util-linux
install autoconf213
install perl-Test-Simple
install perl-Config-General

# fonts required for PGO
install xorg-x11-font*

# lots of required packages that we build against.  We need the i686 and x86_64
# versions of each, along with -devel packages, and yum does a poor job of
# figuring out the interdependencies so we list all four.

install alsa-lib-devel.i686
install alsa-lib-devel.x86_64
install alsa-lib.i686
install alsa-lib.x86_64
install atk-devel.i686
install atk-devel.x86_64
install atk.i686
install atk.x86_64
install cairo-devel.i686
install cairo-devel.x86_64
install cairo.i686
install cairo.x86_64
install check-devel.i686
install check-devel.x86_64
install check.i686
install check.x86_64
install dbus-glib-devel.i686
install dbus-glib-devel.x86_64
install dbus-glib.i686
install dbus-glib.x86_64
install fontconfig-devel.i686
install fontconfig-devel.x86_64
install fontconfig.i686
install fontconfig.x86_64
install freetype-devel.i686
install freetype-devel.x86_64
install freetype.i686
install freetype.x86_64
install GConf2-devel.i686
install GConf2-devel.x86_64
install GConf2.i686
install GConf2.x86_64
install gdk-pixbuf2-devel.i686
install gdk-pixbuf2-devel.x86_64
install glib2-devel.i686
install glib2-devel.x86_64
install glib2.i686
install glib2.x86_64
install glibc-devel.i686
install glibc-devel.x86_64
install glibc.i686
install glibc.x86_64
install gnome-vfs2-devel.i686
install gnome-vfs2-devel.x86_64
install gnome-vfs2.i686
install gnome-vfs2.x86_64
install gstreamer-devel.i686
install gstreamer-devel.x86_64
install gstreamer.i686
install gstreamer-plugins-base-devel.i686
install gstreamer-plugins-base-devel.x86_64
install gstreamer-plugins-base.i686
install gstreamer-plugins-base.x86_64
install gstreamer.x86_64
install gtk2-devel.i686
install gtk2-devel.x86_64
install gtk2.i686
install gtk2.x86_64
install libcurl-devel.i686
install libcurl-devel.x86_64
install libcurl.i686
install libcurl.x86_64
install libdrm-devel.i686
install libdrm-devel.x86_64
install libdrm.i686
install libdrm.x86_64
install libICE-devel.i686
install libICE-devel.x86_64
install libICE.i686
install libICE.x86_64
install libIDL-devel.i686
install libIDL-devel.x86_64
install libIDL.i686
install libIDL.x86_64
install libidn-devel.i686
install libidn-devel.x86_64
install libidn.i686
install libidn.x86_64
install libnotify-devel.i686
install libnotify-devel.x86_64
install libnotify.i686
install libnotify.x86_64
install libpng-devel.i686
install libpng-devel.x86_64
install libpng.i686
install libpng.x86_64
install libSM-devel.i686
install libSM-devel.x86_64
install libSM.i686
install libSM.x86_64
install libstdc++-devel.i686
install libstdc++-devel.x86_64
install libstdc++.i686
install libstdc++.x86_64
install libX11-devel.i686
install libX11-devel.x86_64
install libX11.i686
install libX11.x86_64
install libXau-devel.i686
install libXau-devel.x86_64
install libXau.i686
install libXau.x86_64
install libxcb-devel.i686
install libxcb-devel.x86_64
install libxcb.i686
install libxcb.x86_64
install libXcomposite-devel.i686
install libXcomposite-devel.x86_64
install libXcomposite.i686
install libXcomposite.x86_64
install libXcursor-devel.i686
install libXcursor-devel.x86_64
install libXcursor.i686
install libXcursor.x86_64
install libXdamage-devel.i686
install libXdamage-devel.x86_64
install libXdamage.i686
install libXdamage.x86_64
install libXdmcp-devel.i686
install libXdmcp-devel.x86_64
install libXdmcp.i686
install libXdmcp.x86_64
install libXext-devel.i686
install libXext-devel.x86_64
install libXext.i686
install libXext.x86_64
install libXfixes-devel.i686
install libXfixes-devel.x86_64
install libXfixes.i686
install libXfixes.x86_64
install libXft-devel.i686
install libXft-devel.x86_64
install libXft.i686
install libXft.x86_64
install libXi-devel.i686
install libXi-devel.x86_64
install libXi.i686
install libXinerama-devel.i686
install libXinerama-devel.x86_64
install libXinerama.i686
install libXinerama.x86_64
install libXi.x86_64
install libxml2-devel.i686
install libxml2-devel.x86_64
install libxml2.i686
install libxml2.x86_64
install libXrandr-devel.i686
install libXrandr-devel.x86_64
install libXrandr.i686
install libXrandr.x86_64
install libXrender-devel.i686
install libXrender-devel.x86_64
install libXrender.i686
install libXrender.x86_64
install libXt-devel.i686
install libXt-devel.x86_64
install libXt.i686
install libXt.x86_64
install libXxf86vm-devel.i686
install libXxf86vm-devel.x86_64
install libXxf86vm.i686
install libXxf86vm.x86_64
install mesa-libGL-devel.i686
install mesa-libGL-devel.x86_64
install mesa-libGL.i686
install mesa-libGL.x86_64
install ORBit2-devel.i686
install ORBit2-devel.x86_64
install ORBit2.i686
install ORBit2.x86_64
install pango-devel.i686
install pango-devel.x86_64
install pango.i686
install pango.x86_64
install pixman-devel.i686
install pixman-devel.x86_64
install pixman.i686
install pixman.x86_64
install pulseaudio-libs-devel.i686
install pulseaudio-libs-devel.x86_64
install pulseaudio-libs.i686
install pulseaudio-libs.x86_64
install wireless-tools-devel.i686
install wireless-tools-devel.x86_64
install wireless-tools.i686
install wireless-tools.x86_64
install zlib-devel.i686
install zlib-devel.x86_64
install zlib.i686
install zlib.x86_64

# x86_64 only packages
install hal-devel.x86_64
install hal.x86_64
install perl-devel.x86_64
install perl.x86_64
install dbus-x11.x86_64

# glibc-static has no -devel
install glibc-static.i686
install glibc-static.x86_64

# dbus-devel comes in two architectures, although dbus does not
install dbus-devel.i686
install dbus-devel.x86_64
install dbus.x86_64

# required for the Python build, below
install bzip2-devel
install openssl-devel
install xz-libs
install sqlite-devel

# required for the git build, below
install autoconf
install perl-devel
install perl-ExtUtils-MakeMaker
install gettext-devel

# build utilities
install ccache

# a basic node environment so that we can run TaskCluster tools
install nodejs
install npm

# enough X to run `make check` and do a PGO build
install Xvfb
install xvinfo

# required for building OS X tools
install patch
install libuuid-devel
install openssl-static
install cmake
install subversion
run
EOF

BUILD=/root/build
mkdir $BUILD

# for the builds below, there's no sense using ccache
export CCACHE_DISABLE=1

cd $BUILD
curl https://raw.githubusercontent.com/mozilla/build-tooltool/master/tooltool.py > tooltool.py

tooltool_fetch() {
    cat >manifest.tt
    python2.7 $BUILD/tooltool.py fetch
    rm manifest.tt
}

# For a few packges, we want to run the very latest, which is hard to find for
# stable old CentOS 6.  Custom yum repostiories are cumbersome and can cause
# unhappy failures when they contain multiple versions of the same package.  So
# we either build from source or install an RPM from tooltool (the former being
# the preferred solution for transparency).  Each of these source files was
# downloaded directly from the upstream project site, although the RPMs are of
# unknown origin.

cd $BUILD
tooltool_fetch <<'EOF'
[
{
    "size": 830601,
    "digest": "c04dadf29a3ac676e93cb684b619f753584f8414167135eb766602671d08c85d7bc564511310564bdf2651d72da911b017f0969b9a26d84df724aebf8733f268",
    "algorithm": "sha512",
    "filename": "yasm-1.1.0-1.x86_64.rpm"
}
]
EOF
yum install -y yasm-*.rpm

# Valgrind
# Install valgrind from sources to make sure we don't strip symbols
tooltool_fetch <<'EOF'
[
{
    "size": 14723076,
    "digest": "34e1013cd3815d30a459b86220e871bb0a6209cc9e87af968f347083693779f022e986f211bdf1a5184ad7370cde12ff2cfca8099967ff94732970bd04a97009",
    "algorithm": "sha512",
    "filename": "valgrind-3.13.0.tar.bz2"
}
]
EOF

valgrind_version=3.13.0
tar -xjf valgrind-$valgrind_version.tar.bz2
cd valgrind-$valgrind_version

# This patch by Julian Seward allows us to write a suppression for
# a leak in a library that gets unloaded before shutdown.
# ref: https://bugs.kde.org/show_bug.cgi?id=79362
patch -p0 < /tmp/valgrind-epochs.patch

./configure --prefix=/usr
make -j"$(grep -c ^processor /proc/cpuinfo)" install

# Git
cd $BUILD
# NOTE: rc builds are in https://www.kernel.org/pub/software/scm/git/testing/
tooltool_fetch <<'EOF'
[
{
    "size": 3938976,
    "visibility": "public",
    "digest": "f31cedb6d7c813d5cc9f40daa54ec6b34b046b8ec1b7a09a37598637f747449147a22736e95e4388d1a29fd01d7974b82342114b91d63b9d5df163ea3659fe72",
    "algorithm": "sha512",
    "filename": "git-2.8.0.rc3.tar.xz",
    "unpack": true
}
]
EOF
cd git-2.8.0.rc3
make configure
./configure --prefix=/usr --without-tcltk
make all install
git config --global user.email "nobody@mozilla.com"
git config --global user.name "mozilla"

# Python
cd $BUILD
tooltool_fetch <<'EOF'
[
{
    "size": 12250696,
    "digest": "67615a6defbcda062f15a09f9dd3b9441afd01a8cc3255e5bc45b925378a0ddc38d468b7701176f6cc153ec52a4f21671b433780d9bde343aa9b9c1b2ae29feb",
    "algorithm": "sha512",
    "filename": "Python-2.7.10.tar.xz",
    "unpack": true
}
]
EOF
cd Python-2.7.10
./configure --prefix=/usr
make
# `altinstall` means that /usr/bin/python still points to CentOS's Python 2.6 install.
# If you want Python 2.7, use `python2.7`
make altinstall

# Enough python utilities to get "peep" working
cd $BUILD
tooltool_fetch <<'EOF'
[
{
    "size": 630700,
    "digest": "1367f3a10c1fef2f8061e430585f1927f6bd7c416e764d65cea1f4255824d549efa77beef8ff784bbd62c307b4b1123502e7b3fd01a243c0cc5b433a841cc8b5",
    "algorithm": "sha512",
    "filename": "setuptools-18.1.tar.gz",
    "unpack": true
},
{
    "size": 1051205,
    "digest": "e7d2e003ec60fce5a75a6a23711d7f9b155e898faebcf55f3abdd912ef513f4e0cf43daca8f9da7179a7a4efe6e4a625a532d051349818847df1364eb5b326de",
    "algorithm": "sha512",
    "filename": "pip-6.1.1.tar.gz",
    "unpack": true
},
{
    "size": 26912,
    "digest": "9d730ed7852d4d217aaddda959cd5f871ef1b26dd6c513a3780bbb04a5a93a49d6b78e95c2274451a1311c10cc0a72755b269dc9af62640474e6e73a1abec370",
    "algorithm": "sha512",
    "filename": "peep-2.4.1.tar.gz",
    "unpack": false
}
]
EOF

cd $BUILD
cd setuptools-18.1
python2.7 setup.py install
# NOTE: latest peep is not compatible with pip>=7.0
# https://github.com/erikrose/peep/pull/94

cd $BUILD
cd pip-6.1.1
python2.7 setup.py install

cd $BUILD
pip2.7 install peep-2.4.1.tar.gz

# Peep (latest)
cd $BUILD
pip2.7 install peep

# remaining Python utilities are installed with `peep` from upstream
# repositories; peep verifies file integrity for us
cat >requirements.txt <<'EOF'
# sha256: 90pZQ6kAXB6Je8-H9-ivfgDAb6l3e5rWkfafn6VKh9g
virtualenv==13.1.2

# sha256: wJnELXTi1SC2HdNyzZlrD6dgXAZheDT9exPHm5qaWzA
mercurial==3.7.3
EOF
peep install -r requirements.txt

# TC-VCS
npm install -g taskcluster-vcs@2.3.18

# Ninja
cd $BUILD
tooltool_fetch <<'EOF'
[
{
    "size": 174501,
    "digest": "551a9e14b95c2d2ddad6bee0f939a45614cce86719748dc580192dd122f3671e3d95fd6a6fb3facb2d314ba100d61a004af4df77f59df119b1b95c6fe8c38875",
    "algorithm": "sha512",
    "filename": "ninja-1.6.0.tar.gz",
    "unpack": true
}
]
EOF
cd ninja-1.6.0
./configure.py --bootstrap
cp ninja /usr/local/bin/ninja
# Old versions of Cmake can only find ninja in this location!
ln -s /usr/local/bin/ninja /usr/local/bin/ninja-build

# note that TC will replace workspace with a cache mount; there's no sense
# creating anything inside there
mkdir -p /builds/worker/workspace
chown worker:worker /builds/worker/workspace

# /builds is *not* replaced with a mount in the docker container. The worker
# user writes to lots of subdirectories, though, so it's owned by that user
mkdir -p /builds
chown worker:worker /builds

# remove packages installed for the builds above
yum shell -y <<'EOF'
remove bzip2-devel
remove openssl-devel
remove xz-libs
remove autoconf
remove perl-ExtUtils-MakeMaker
remove gettext-devel
remove sqlite-devel
remove perl-devel
EOF

# clean up caches from all that downloading and building
cd /
rm -rf $BUILD ~/.ccache ~/.cache ~/.npm
yum clean all
rm "$0"
