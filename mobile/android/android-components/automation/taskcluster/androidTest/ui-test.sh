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
set -ex

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
    echo "To run UI samples/sampleName tests"
    echo "$ execute-firebase-test.sh sample-sampleName arm 1"
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
ARTIFACT_DIR="/builds/worker/artifacts"
RESULTS_DIR="${ARTIFACT_DIR}/results"

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

# Remove samples- from the component for each APK path
samples=${component//samples-}

# If tests are for components, the path is different than for samples
if [[ "${component}" != samples-* ]]
then
    # Case 1: tests for any component (but NOT samples, NOT real UI tests)
    APK_APP="./samples/browser/build/outputs/apk/gecko/debug/samples-browser-gecko-debug.apk"
    if [[ "${component}" == *"-"* ]]
    then
      regex='([a-z]*)-(.*)'
      [[ "$component" =~ $regex ]]
      APK_TEST="./components/${BASH_REMATCH[1]}/${BASH_REMATCH[2]}/build/outputs/apk/androidTest/debug/${component}-debug-androidTest.apk"
      else
        APK_TEST="./components/${component}/engine-gecko/build/outputs/apk/androidTest/debug/browser-engine-gecko-debug-androidTest.apk"
    fi
elif [[ "${component}" == "samples-browser" ]]
then
    # Case 2: tests for browser sample (gecko sample only)
    APK_APP="./samples/${samples}/build/outputs/apk/gecko/debug/samples-${samples}-gecko-debug.apk"
    APK_TEST="./samples/${samples}/build/outputs/apk/androidTest/gecko/debug/samples-{$samples}-gecko-debug-androidTest.apk"
else
    # Case 3: tests for non-browser samples (i.e.  samples-glean)
    APK_APP="./samples/${samples}/build/outputs/apk/debug/samples-${samples}-debug.apk"
    APK_TEST="./samples/${samples}/build/outputs/apk/androidTest/debug/samples-${samples}-debug-androidTest.apk"
fi

# function to exit script with exit code from test run.
# (Only 0 if all test executions passed)
function failure_check() {
    echo
    echo
    if [[ $exitcode -ne 0 ]]; then
        echo "ERROR: UI test run failed, please check above URL"
    else
    echo "All UI test(s) have passed!"
    fi

    echo
    echo "RESULTS"
    echo
    ls -la "${RESULTS_DIR}"
    echo
    echo
}

echo
echo "EXECUTE TEST(S)"
echo
$JAVA_BIN -jar $FLANK_BIN android run --config=$flank_template --max-test-shards=$num_shards --app=$APK_APP --test=$APK_TEST --project=$GOOGLE_PROJECT --local-result-dir="${RESULTS_DIR}"
exitcode=$?

failure_check
exit $exitcode
