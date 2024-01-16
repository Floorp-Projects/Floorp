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

# TODO: switch to offline builds
export GRADLE_MAVEN_REPOSITORIES="file://$MOZ_FETCHES_DIR/geckoview","https://maven.mozilla.org/maven2/","https://maven.google.com/","https://repo.maven.apache.org/maven2/","https://plugins.gradle.org/m2/"
EOF
export MOZCONFIG=$mozconfig

./mach configure

eval $PRE_GRADLEW

eval $GET_SECRETS

./gradlew listRepositories $GRADLEW_ARGS

eval $POST_GRADLEW
