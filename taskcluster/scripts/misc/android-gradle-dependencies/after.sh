#!/bin/bash -vex

set -x -e

echo "running as" $(id)

: WORKSPACE ${WORKSPACE:=/builds/worker/workspace}
: GRADLE_VERSION ${GRADLE_VERSION:=3.4.1}

set -v

# Package everything up.
pushd $WORKSPACE
mkdir -p android-gradle-dependencies /builds/worker/artifacts

cp -R ${NEXUS_WORK}/storage/jcenter android-gradle-dependencies
cp -R ${NEXUS_WORK}/storage/google android-gradle-dependencies

# The Gradle wrapper will have downloaded and verified the hash of exactly one
# Gradle distribution.  It will be located in $GRADLE_USER_HOME, like
# ~/.gradle/wrapper/dists/gradle-2.7-all/$PROJECT_HASH/gradle-2.7-all.zip.  We
# want to remove the version from the internal directory for use via tooltool in
# a mozconfig.
cp ${GRADLE_USER_HOME}/wrapper/dists/gradle-${GRADLE_VERSION}-all/*/gradle-${GRADLE_VERSION}-all.zip gradle-${GRADLE_VERSION}-all.zip
unzip -q gradle-${GRADLE_VERSION}-all.zip
mv gradle-${GRADLE_VERSION} android-gradle-dependencies/gradle-dist

tar cf - android-gradle-dependencies | xz > /builds/worker/artifacts/android-gradle-dependencies.tar.xz

popd
