#!/usr/bin/env bash

set -ve

test `whoami` == 'root'

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
install zlib-devel
install perl-Test-Simple
install perl-Config-General
install xorg-x11-font*  # fonts required for PGO
install dbus-x11
install libstdc++
install libstdc++.i686

# lots of required -devel packages
install libnotify-devel
install alsa-lib-devel
install libcurl-devel
install wireless-tools-devel
install libX11-devel
install libXt-devel
install mesa-libGL-devel
install GConf2-devel
install pulseaudio-libs-devel
install gstreamer-devel
install gstreamer-plugins-base-devel

# Prerequisites for GNOME that are not included in tooltool
install libpng-devel
install libXrandr-devel

# required for the Python build, below
install bzip2-devel
install openssl-devel
install xz-libs
install sqlite-devel

# required for the git build, below
install autoconf
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
    python $BUILD/tooltool.py fetch
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
    "size": 383372,
    "digest": "486c34aeb01d2d083fdccd043fb8e3af93efddc7c185f38b5d7e90199733424c538ac2a7dfc19999c82ff00b84de5df75aa6f891e622cf1a86d698ed0bee16e2",
    "algorithm": "sha512",
    "filename": "freetype-2.4.12-6.el6.1.x86_64.rpm"
},
{
    "size": 397772,
    "digest": "79c7ddc943394509b96593f3aeac8946acd9c9dc10a3c248a783cb282781cbc9e4e7c47b98121dd7849d39aa3bd29313234ae301ab80f769010c2bacf41e6d40",
    "algorithm": "sha512",
    "filename": "freetype-devel-2.4.12-6.el6.1.x86_64.rpm"
},
{
    "size": 17051332,
    "digest": "57c816a6df9731aa5f34678abb59ea560bbdb5abd01df3f3a001dc94a3695d3190b1121caba483f8d8c4a405f4e53fde63a628527aca73f05652efeaec9621c4",
    "algorithm": "sha512",
    "filename": "valgrind-3.10.0-1.x86_64.rpm"
},
{
    "size": 830601,
    "digest": "c04dadf29a3ac676e93cb684b619f753584f8414167135eb766602671d08c85d7bc564511310564bdf2651d72da911b017f0969b9a26d84df724aebf8733f268",
    "algorithm": "sha512",
    "filename": "yasm-1.1.0-1.x86_64.rpm"
}
]
EOF
yum install -y freetype-*.rpm
yum install -y valgrind-*.rpm
yum install -y yasm-*.rpm

# The source RPMs for some of those; not used here, but included for reference
: <<'EOF'
[
{
    "size": 10767445,
    "digest": "d435897b602f7bdf77fabf1c80bbd06ba4f7288ad0ef31d19a863546d4651172421b45f2f090bad3c3355c9fa2a00352066f18d99bf994838579b768b90553d3",
    "algorithm": "sha512",
    "filename": "valgrind-3.10.0-1.src.rpm"
},
{
    "size": 1906249,
    "digest": "c3811c74e4ca3c342e6625cdb80a62903a14c56436774149754de67f41f79ee63248c71d039dd9341d22bdf4ef8b7dc8c90fe63dd9a3df44197d38a2d3d8f4a2",
    "algorithm": "sha512",
    "filename": "freetype-2.4.12-6.el6.1.src.rpm"
}
]
EOF

# Git
cd $BUILD
tooltool_fetch <<'EOF'
[
{
    "size": 3740620,
    "digest": "ef7538c9f5ba5b2ac08962401c30e5fd51323b54b9fb5315d259adccec346e8fae9362815832dc2b5ce63a259b315c40e419bb2385dde04d84b992e62f6789b6",
    "algorithm": "sha512",
    "unpack": true,
    "filename": "git-2.5.0.tar.xz"
}
]
EOF
cd git-2.5.0
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

# sha256: tQ9peOfTn-DLKY-j-j6c5B0jVnIdFV5SiPnFfl8T6ac
mercurial==3.5
EOF
peep install -r requirements.txt

# TC-VCS
npm install -g taskcluster-vcs@2.3.8

# note that TC will replace workspace with a cache mount; there's no sense
# creating anything inside there
mkdir -p /home/worker/workspace
chown worker:worker /home/worker/workspace

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
EOF

# clean up caches from all that downloading and building
cd /
rm -rf $BUILD ~/.ccache ~/.cache ~/.npm
yum clean all
rm $0
