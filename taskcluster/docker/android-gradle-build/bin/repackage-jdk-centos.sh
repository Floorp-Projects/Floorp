#! /bin/bash

set -e -x

: WORKSPACE ${WORKSPACE:=/workspace}

set -v

mkdir -p $WORKSPACE/java
pushd $WORKSPACE/java

# change these variables when updating java version
mirror_url_base="http://mirror.centos.org/centos/6/os/x86_64/Packages"
openjdk=java-1.8.0-openjdk-headless-1.8.0.121-1.b13.el6.x86_64.rpm
openjdk_devel=java-1.8.0-openjdk-devel-1.8.0.121-1.b13.el6.x86_64.rpm
jvm_openjdk_dir=java-1.8.0-openjdk-1.8.0.121-1.b13.el6.x86_64

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

# tzdb.dat is an absolute symlink, which might not work when we
# repackage.  Copy the underlying timezone database.  Copying the
# target file from the toolchain producing system avoids requiring the
# consuming system to have java-tzdb installed, which would bake in a
# subtle dependency that has been addressed in modern versions of
# CentOS.  See https://bugzilla.redhat.com/show_bug.cgi?id=1130800 for
# discussion.
rm java_home/jre/lib/tzdb.dat
cp /usr/share/javazi-1.8/tzdb.dat java_home/jre/lib/tzdb.dat

# document version this is based on
echo "Built from ${mirror_url_Base}
    ${openjdk}
    ${openjdk_devel}

Run through rpm2cpio | cpio, and /usr/lib/jvm/${jvm_openjdk_dir} renamed to 'java_home'." > java_home/VERSION

popd
