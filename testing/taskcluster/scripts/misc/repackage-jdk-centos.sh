#! /bin/bash

set -e -x

mkdir artifacts
cd build

rm -rf root && mkdir root && cd root

# change these variables when updating java version
mirror_url_base="http://mirror.centos.org/centos/6.7/updates/x86_64/Packages"
openjdk=java-1.7.0-openjdk-1.7.0.85-2.6.1.3.el6_7.x86_64.rpm
openjdk_devel=java-1.7.0-openjdk-1.7.0.85-2.6.1.3.el6_7.x86_64.rpm
jvm_openjdk_dir=java-1.7.0-openjdk-1.7.0.85.x86_64

# grab the rpm and unpack it
wget ${mirror_url_base}/${openjdk}
wget ${mirror_url_base}/${openjdk_devel}
rpm2cpio $openjdk | cpio -ivd
rpm2cpio $openjdk_devel | cpio -ivd

cd usr/lib/jvm
mv $jvm_openjdk_dir java_home

# document version this is based on
echo "Built from ${mirror_url_Base}
    ${openjdk}
    ${openjdk_devel}

Run through rpm2cpio | cpio, and /usr/lib/jvm/${jvm_openjdk_dir} renamed to 'java_home'." > java_home/VERSION

# tarball the unpacked rpm and put it in the taskcluster upload artifacts dir
tar -Jvcf ~/artifacts/java_home-${jvm_openjdk_dir}.tar.xz java_home
