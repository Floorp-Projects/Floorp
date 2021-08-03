#!/bin/bash -vex

set -x -e

echo "running as" $(id)

: WORKSPACE ${WORKSPACE:=/builds/worker/workspace}

set -v

# Package everything up.
pushd $WORKSPACE
mkdir -p android-gradle-dependencies /builds/worker/artifacts

# NEXUS_WORK is exported by `before.sh`.
cp -R ${NEXUS_WORK}/storage/central android-gradle-dependencies
cp -R ${NEXUS_WORK}/storage/google android-gradle-dependencies
cp -R ${NEXUS_WORK}/storage/gradle-plugins android-gradle-dependencies

# The Gradle wrapper will have downloaded and verified the hash of exactly one
# Gradle distribution.  It will be located in $GRADLE_USER_HOME, like
# ~/.gradle/wrapper/dists/gradle-2.7-all/$PROJECT_HASH/gradle-2.7-all.zip.  We
# want to remove the version from the internal directory for use via tooltool in
# a mozconfig.
cp ${GRADLE_USER_HOME}/wrapper/dists/gradle-*-*/*/gradle-*-*.zip gradle.zip
unzip -q gradle.zip
mv gradle-* android-gradle-dependencies/gradle-dist

tar cavf /builds/worker/artifacts/android-gradle-dependencies.tar.zst android-gradle-dependencies

popd
