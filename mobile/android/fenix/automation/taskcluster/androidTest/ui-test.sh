#!/usr/bin/env bash
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This script does the following:
# 1. Retrieves gcloud service account token
# 2. Activates gcloud service account
# 3. Connects to google Firebase (using TestArmada's Flank tool)
# 4. Executes UI tests
# 5. Puts test artifacts into the test_artifacts folder

# NOTE:
# Flank supports sharding across multiple devices at a time, but gcloud API
# only supports 1 defined APK per test run.


# If a command fails then do not proceed and fail this script too.
set -e

#########################
# The command line help #
#########################
display_help() {
    echo "Usage: $0 Build_Variant [Number_Shards...]"
    echo
    echo "Examples:"
    echo "To run UI tests on ARM device shard (1 test / shard)"
    echo "$ ui-test.sh arm64-v8a -1"
    echo
    echo "To run UI tests on X86 device (on 3 shards)"
    echo "$ ui-test.sh x86 3"
    echo
}

get_abs_filename() {
  relative_filename="$1"
  echo "$(cd "$(dirname "$relative_filename")" && pwd)/$(basename "$relative_filename")"
}


# Basic parameter check
if [[ $# -lt 1 ]]; then
    echo "Error: please provide at least one build variant (arm|x86)"
    display_help
    exit 1
fi

device_type="$1"  # arm64-v8a | armeabi-v7a | x86_64 | x86
APK_APP="$2"
APK_TEST="$3"
if [[ ! -z "$4" ]]; then
    num_shards=$4
fi

JAVA_BIN="/usr/bin/java"
PATH_TEST="./automation/taskcluster/androidTest"
FLANK_BIN="/builds/worker/test-tools/flank.jar"

echo
echo "ACTIVATE SERVICE ACCT"
echo
# this is where the Google Testcloud project ID is set
gcloud config set project "$GOOGLE_PROJECT"
echo

gcloud auth activate-service-account --key-file "$GOOGLE_APPLICATION_CREDENTIALS"
echo
echo

# Disable exiting on error. If the tests fail we want to continue
# and try to download the artifacts. We will exit with the actual error code later.
set +e

if [[ "${device_type}" =~ ^(arm64-v8a|armeabi-v7a|x86_64|x86)$ ]]; then
    flank_template="${PATH_TEST}/flank-${device_type}.yml"
    echo "device_type: ${device_type}"
else
    echo "NOT FOUND"
    exitcode=1
fi

APK_APP="$(get_abs_filename $APK_APP)"
APK_TEST="$(get_abs_filename $APK_TEST)"
echo "APK_APP: ${APK_APP}"
echo "APK_TEST: ${APK_TEST}"

# function to exit script with exit code from test run.
# (Only 0 if all test executions passed)
function failure_check() {
    if [[ $exitcode -ne 0 ]]; then
        echo
        echo
        echo "ERROR: UI test run failed, please check above URL"
    fi
    exit $exitcode
}

echo
echo "FLANK VERSION"
echo
$JAVA_BIN -jar $FLANK_BIN --version
echo
echo

echo
echo "EXECUTE TEST(S)"
echo
$JAVA_BIN -jar $FLANK_BIN android run --config=$flank_template --max-test-shards=$num_shards --app=$APK_APP --test=$APK_TEST --project=$GOOGLE_PROJECT
exitcode=$?
failure_check
echo
echo

echo
echo "COPY ARTIFACTS"
echo
cp -r ./results ./test_artifacts
exitcode=$?
failure_check
echo
echo

echo
echo "RESULTS"
echo
ls -la ./results
echo
echo

echo "All UI test(s) have passed!"
echo
echo
