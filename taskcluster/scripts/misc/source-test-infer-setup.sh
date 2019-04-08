#!/bin/bash
source $HOME/checkouts/gecko/taskcluster/scripts/misc/source-test-common.sh

# Write custom mozconfig
MOZCONFIG=$HOME/checkouts/gecko/mozconfig
echo "ac_add_options --enable-application=mobile/android" > $MOZCONFIG
echo "ac_add_options --target=arm-linux-androideabi" >> $MOZCONFIG
echo "ac_add_options --with-android-sdk=${MOZBUILD_STATE_PATH}/android-sdk-linux" >> $MOZCONFIG
echo "ac_add_options --with-android-ndk=${MOZBUILD_STATE_PATH}/android-ndk" >> $MOZCONFIG

# Write custom grade properties
export GRADLE_USER_HOME=$HOME/workspace/gradle
mkdir -p $GRADLE_USER_HOME
echo "org.gradle.daemon=false" >> ${GRADLE_USER_HOME}/gradle.properties

# Mach lookup infer in infer...
mkdir -p $MOZBUILD_STATE_PATH/infer/infer
mv $MOZBUILD_STATE_PATH/infer/{bin,lib} $MOZBUILD_STATE_PATH/infer/infer
