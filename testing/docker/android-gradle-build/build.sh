#!/bin/bash -vex

set -x -e

: WORKSPACE ${WORKSPACE:=/workspace}
: GRADLE_VERSION ${GRADLE_VERSION:=2.7}

set -v

# Frowned upon, but simplest.
RUN_AS_USER=root NEXUS_WORK=${WORKSPACE}/nexus /opt/sonatype/nexus/bin/nexus start

# Wait "a while" for Nexus to actually start.  Don't fail if this fails.
wget --quiet --retry-connrefused --waitretry=2 --tries=100 \
  http://localhost:8081/nexus/service/local/status || true
rm -rf status

# Verify Nexus has actually started.  Fail if this fails.
curl --fail --silent --location http://localhost:8081/nexus/service/local/status | grep '<state>STARTED</state>'

export JAVA_HOME=/usr/lib/jvm/jre-1.7.0-openjdk

pushd /project
./gradlew tasks
popd

# Package everything up.
pushd ${WORKSPACE}
# Not yet.  See notes on tooltool below.
# cp -R /root/.android-sdk android-sdk-linux
# tar cJf android-sdk-linux.tar.xz android-sdk-linux

cp -R /workspace/nexus/storage/central jcentral
tar cJf jcentral.tar.xz jcentral

# The Gradle wrapper will have downloaded and verified the hash of exactly one
# Gradle distribution.  It will be located at
# ~/.gradle/wrapper/dists/gradle-2.7-all/$PROJECT_HASH/gradle-2.7-all.zip.  We
# want to remove the version from the internal directory for use via tooltool
# in a mozconfig.
cp ~/.gradle/wrapper/dists/gradle-${GRADLE_VERSION}-all/*/gradle-${GRADLE_VERSION}-all.zip gradle-${GRADLE_VERSION}-all.zip
unzip -q gradle-${GRADLE_VERSION}-all.zip
mv gradle-${GRADLE_VERSION} gradle
tar cJf gradle.tar.xz gradle

mkdir -p /artifacts
# We can't redistribute the Android SDK publicly just yet.  We'll
# upload to (internal) tooltool eventually.  mv
# android-sdk-linux.tar.xz /artifacts
mv jcentral.tar.xz /artifacts
mv gradle.tar.xz /artifacts
popd

# Bug 1245170: at some point in the future, we'll be able to upload
# things directly to tooltool.
# pushd /artifacts
# /build/tooltool.py add --visibility=public jcentral.tar.xz
# /build/tooltool.py add --visibility=public gradle.tar.xz
# /build/tooltool.py add --visibility=internal android-sdk-linux.tar.xz
# /build/tooltool.py upload -v --url=http://relengapi/tooltool/ \
#   --message="No message - Gradle and jcentral archives uploaded from taskcluster."
# popd
