#!/usr/bin/env bash
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This script installs google cloud sdk, logs into google firebase, executes tests,
# and collects test artifacts into the test_artifacts folder

# If a command fails then do not proceed and fail this script too.
set -ex
#########################
# The command line help #
#########################
display_help() {
    echo "Usage: $0 Build_Variant Apk_Name Device_Config [Device_Config...]" >&2
    echo
    echo "All Build Variants must be of Debug type, so the word 'debug' should be omitted"
    echo "E.g. KlarGeckoX86Debug should be KlarGeckoX86"
    echo
    echo "Do not put .apk prefix in Apk_Name"
}

# Basic parameter check
if [ $# -lt 3 ]; then
    echo "Your command line contains $# arguments"
    display_help
    exit 1
fi

# Get test configuration
config_count=`expr $# - 2`
config_param="--device $3"
for ((i=4; i <= $#; i++))
{
    config_param+=" --device ${!i}"
}

# From now on disable exiting on error. If the tests fail we want to continue
# and try to download the artifacts. We will exit with the actual error code later.
set +e

# Execute test set
/google-cloud-sdk/bin/gcloud --format="json" firebase test android run \
--type instrumentation \
--app app/build/outputs/apk/$1/debug/$2.apk \
--test app/build/outputs/apk/androidTest/$1/debug/$2-androidTest.apk \
--results-bucket focus_android_test_artifacts \
--timeout 30m \
--no-auto-google-login \
--test-runner-class android.support.test.runner.AndroidJUnitRunner $config_param

exitcode=$?

echo "Downloading artifacts"

mkdir test_artifacts
/google-cloud-sdk/bin/gsutil ls gs://focus_android_test_artifacts | tail -1 | /google-cloud-sdk/bin/gsutil -m cp -r -I ./test_artifacts

# Now exit the script with the exit code from the test run. (Only 0 if all test executions passed)
exit $exitcode
