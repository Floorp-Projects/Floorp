#!/bin/bash -vex

set -x -e

echo "running as" $(id)

: WORKSPACE ${WORKSPACE:=/home/worker/workspace}
: GRADLE_VERSION ${GRADLE_VERSION:=2.14.1}

set -v

# Package everything up.
pushd $WORKSPACE

cp -R /home/worker/.mozbuild/android-sdk-linux android-sdk-linux
tar cJf android-sdk-linux.tar.xz android-sdk-linux

# We can't redistribute the Android SDK publicly.
mkdir -p /home/worker/private/android-sdk
mv android-sdk-linux.tar.xz /home/worker/private/android-sdk

cp -R $WORKSPACE/build/src/java_home java_home
tar cJf java_home.tar.xz java_home

# We can't redistribute Java publicly.
mkdir -p /home/worker/private/java_home
mv java_home.tar.xz /home/worker/private/java_home

cp -R $WORKSPACE/nexus/storage/jcenter jcenter
tar cJf jcenter.tar.xz jcenter

cp -R $WORKSPACE/nexus/storage/google google
tar cJf google.tar.xz google

# The Gradle wrapper will have downloaded and verified the hash of exactly one
# Gradle distribution.  It will be located in $GRADLE_USER_HOME, like
# ~/.gradle/wrapper/dists/gradle-2.7-all/$PROJECT_HASH/gradle-2.7-all.zip.  We
# want to remove the version from the internal directory for use via tooltool in
# a mozconfig.
cp $GRADLE_USER_HOME/wrapper/dists/gradle-${GRADLE_VERSION}-all/*/gradle-${GRADLE_VERSION}-all.zip gradle-${GRADLE_VERSION}-all.zip
unzip -q gradle-${GRADLE_VERSION}-all.zip
mv gradle-${GRADLE_VERSION} gradle-dist
tar cJf gradle-dist.tar.xz gradle-dist

mkdir -p /home/worker/artifacts
mv jcenter.tar.xz /home/worker/artifacts
mv google.tar.xz /home/worker/artifacts
mv gradle-dist.tar.xz /home/worker/artifacts
popd
