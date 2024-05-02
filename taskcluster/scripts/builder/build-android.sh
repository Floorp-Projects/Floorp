#!/bin/sh

set -e
set -x

mozconfig=$(mktemp)
cat > $mozconfig <<EOF
# the corresponding geckoview's mozconfig, to pick up its config options
. $MOZCONFIG
# no-compile because we don't need to build native code here
. $GECKO_PATH/build/mozconfig.no-compile

# Disable Keyfile Loading (and checks)
# This overrides the settings in the common android mozconfig
ac_add_options --without-mozilla-api-keyfile
ac_add_options --without-google-location-service-api-keyfile
ac_add_options --without-google-safebrowsing-api-keyfile

ac_add_options --disable-nodejs
unset NODEJS

export GRADLE_MAVEN_REPOSITORIES="file://$MOZ_FETCHES_DIR/geckoview","file://$MOZ_FETCHES_DIR/android-gradle-dependencies/mozilla","file://$MOZ_FETCHES_DIR/android-gradle-dependencies/google","file://$MOZ_FETCHES_DIR/android-gradle-dependencies/central","file://$MOZ_FETCHES_DIR/android-gradle-dependencies/gradle-plugins","file:///$MOZ_FETCHES_DIR/android-gradle-dependencies/plugins.gradle.org/m2"
EOF
export MOZCONFIG=$mozconfig
GRADLE=$MOZ_FETCHES_DIR/android-gradle-dependencies/gradle-dist/bin/gradle

./mach configure

eval $PRE_GRADLEW

eval $GET_SECRETS

$GRADLE listRepositories $GRADLEW_ARGS

eval $POST_GRADLEW
