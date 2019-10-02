#!/usr/bin/env bash
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This script does the following:
# 1. Retrieves glcoud service account token
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
    echo "Usage: $0 Component_Name Build_Variant [Number_Shards...]"
    echo
    echo "Examples:"
    echo "To run component/browser tests on ARM device shard (1 test / shard)"
    echo "$ execute-firebase-test.sh component arm"
    echo
    echo "To run component/feature tests on X86 device (on 3 shards)"
    echo "$ execute-firebase-test.sh feature x86 3"
    echo
}

# Basic parameter check
if [[ $# -lt 2 ]]; then
    echo "Your command line contains $# arguments"
    display_help
    exit 1
fi

component="$1" # browser, concept, feature
device_type="$2"  # arm | x86
if [[ ! -z "$3" ]]; then
    num_shards=$3
fi

JAVA_BIN="/usr/bin/java"
PATH_TEST="./automation/taskcluster/androidTest"
FLANK_BIN="/builds/worker/test-tools/flank.jar"
FLANK_CONF_ARM="${PATH_TEST}/flank-arm.yml"
FLANK_CONF_X86="${PATH_TEST}/flank-x86.yml"

echo
echo "ACTIVATE SERVICE ACCT"
echo
# this is where the Google Testcloud project ID is set
gcloud config set project "$GOOGLE_PROJECT"
echo

gcloud auth activate-service-account --key-file "$GOOGLE_APPLICATION_CREDENTIALS"
echo
echo

# From now on disable exiting on error. If the tests fail we want to continue
# and try to download the artifacts. We will exit with the actual error code later.
set +e

if [[ "${device_type,,}" == "x86" ]]
then
    flank_template="$FLANK_CONF_X86"
else
    flank_template="$FLANK_CONF_ARM"
fi

APK_APP="./samples/${component}/build/outputs/apk/geckoNightly/debug/samples-browser-geckoNightly-debug.apk"
APK_TEST="./components/${component}/engine-gecko-nightly/build/outputs/apk/androidTest/debug/browser-engine-gecko-nightly-debug-androidTest.apk"


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
echo "EXECUTE TEST(S)"
echo
$JAVA_BIN -jar $FLANK_BIN android run --config=$flank_template --max-test-shards=$num_shards --app=$APK_APP --test=$APK_TEST --project=$GOOGLE_PROJECT
exitcode=$?

echo
echo
echo "COPY ARTIFACTS"
echo
cp -r "./results" "./test_artifacts"
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

echo
echo "RESULTS"
echo
ls -la ./test_results

echo "All UI test(s) have passed!"
echo
echo


