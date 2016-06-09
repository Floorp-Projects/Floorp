#! /bin/bash

set -e -x

mkdir -p artifacts
pushd build

rm -rf root && mkdir root && cd root

# change these variables when updating java version
mirror_url_base="http://mirror.centos.org/centos/6.7/updates/x86_64/Packages"
openjdk=java-1.7.0-openjdk-1.7.0.85-2.6.1.3.el6_7.x86_64.rpm
openjdk_devel=java-1.7.0-openjdk-devel-1.7.0.85-2.6.1.3.el6_7.x86_64.rpm
jvm_openjdk_dir=java-1.7.0-openjdk-1.7.0.85.x86_64

# grab the rpm and unpack it
wget ${mirror_url_base}/${openjdk}
wget ${mirror_url_base}/${openjdk_devel}
rpm2cpio $openjdk | cpio -ivd
rpm2cpio $openjdk_devel | cpio -ivd

cd usr/lib/jvm
mv $jvm_openjdk_dir java_home

# cacerts is a relative symlink, which doesn't work when we repackage.  Make it
# absolute.  We could use tar's --dereference option, but there's a subtle
# difference between making the symlink absolute and using --dereference.
# Making the symlink absolute lets the consuming system set the cacerts; using
# --dereference takes the producing system's cacerts and sets them in stone.  We
# prefer the flexibility of the former.
rm java_home/jre/lib/security/cacerts
ln -s /etc/pki/java/cacerts java_home/jre/lib/security/cacerts

# document version this is based on
echo "Built from ${mirror_url_Base}
    ${openjdk}
    ${openjdk_devel}

Run through rpm2cpio | cpio, and /usr/lib/jvm/${jvm_openjdk_dir} renamed to 'java_home'." > java_home/VERSION

# tarball the unpacked rpm and put it in the taskcluster upload artifacts dir
tar -Jvcf java_home-${jvm_openjdk_dir}.tar.xz java_home
popd

mv build/root/usr/lib/jvm/java_home-${jvm_openjdk_dir}.tar.xz artifacts
