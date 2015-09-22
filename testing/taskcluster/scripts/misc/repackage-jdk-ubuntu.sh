#! /bin/bash

set -e -x

# VERSION must be supplied to the task; example:
#   VERSION=7u79-2.5.5-0ubuntu0.14.04.2
test VERSION

# this only needs to correspond to the build architecture, which for our purposes
# (building Fennec) is always amd64
arch=amd64

mkdir artifacts
cd build

rm -rf root && mkdir root

for pkg in openjdk-7-{jdk,jre,jre-headless}; do
    curl -v --fail -o the.deb http://mirrors.kernel.org/ubuntu/pool/main/o/openjdk-7/${pkg}_${VERSION}_${arch}.deb
    dpkg-deb -x the.deb root
done

# package up the contents of usr/lib/jvm
(
    cd root/usr/lib/jvm
    # rename the root dir to something non-arch-specific
    mv java-7-openjdk* java_home
    ( echo "VERSION=$VERSION"; echo "arch=$arch" ) > java_home/VERSION
    tar -Jvcf ~/artifacts/java_home-${VERSION}-${arch}.tar.xz java_home
)
