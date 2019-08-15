#!/bin/bash
source $GECKO_PATH/taskcluster/scripts/misc/source-test-common.sh

# Write custom mozconfig
MOZCONFIG=$GECKO_PATH/mozconfig
echo "ac_add_options --enable-application=mobile/android" > $MOZCONFIG
echo "ac_add_options --target=arm-linux-androideabi" >> $MOZCONFIG
echo "ac_add_options --with-android-sdk=${MOZ_FETCHES_DIR}/android-sdk-linux" >> $MOZCONFIG
echo "ac_add_options --with-android-ndk=${MOZ_FETCHES_DIR}/android-ndk" >> $MOZCONFIG

# Write custom grade properties
export GRADLE_USER_HOME=$HOME/workspace/gradle
mkdir -p $GRADLE_USER_HOME
echo "org.gradle.daemon=false" >> ${GRADLE_USER_HOME}/gradle.properties

# Mach lookup infer in infer...
mkdir -p $MOZBUILD_STATE_PATH/infer/infer
mv $MOZ_FETCHES_DIR/infer/{bin,lib} $MOZBUILD_STATE_PATH/infer/infer
