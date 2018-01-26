#!/usr/bin/env bash
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This script installs google cloud sdk, logs into google firebase, executes tests,
# and collects test artifacts into the test_artifacts folder

# If a command fails then do not proceed and fail this script too.
set -e

# set
chmod +x ./tools/taskcluster/install-google-cloud.sh
chmod +x ./tools/taskcluster/google-firebase-testlab-login.sh
./tools/taskcluster/install-google-cloud.sh
./tools/taskcluster/google-firebase-testlab-login.sh

# Compile test builds
./gradlew assembleFocusWebviewDebug assembleFocusWebviewDebugAndroidTest

# Execute test set, this is the configuration that is currently set in BuddyBuild
testresult=`./google-cloud-sdk/bin/gcloud --format="json" firebase test android run --app="app/build/outputs/apk/app-focus-webview-debug.apk" \
--test="app/build/outputs/apk/app-focus-webview-debug-androidTest.apk" \
--results-bucket="focus_android_test_artifacts" --timeout="30m" --no-auto-google-login \
--test-runner-class="android.support.test.runner.AndroidJUnitRunner" \
--device="model=Nexus5X,version=23" --device="model=Nexus9,version=25"\
--device="model=sailfish,version=25" --device="model=sailfish,version=26" `

# Pull the artifacts from TestCloud to taskcluster
# Google storage folder name can be modified from the Google Cloud menu
echo "Download Artifacts"
rm -rf test_artifacts
mkdir test_artifacts
./google-cloud-sdk/bin/gsutil ls gs://focus_android_test_artifacts | tail -1 | ./google-cloud-sdk/bin/gsutil -m cp -r -I ./test_artifacts

# Test passed when:
# There is at least one passed test run, and no failures
echo "Test Execution Result"
shopt -s nocasematch
echo $testresult
if [[ "$testresult" =~ "Passed" ]]; then
    if [[ "$testresult" =~ "Failed" ]]; then
    	  # At least one test case failed
        echo "Test Failed: Failed Test Present"
        exit 1
    else
        echo "Test Passed"
    fi
else
	  # All tests failed, or no output given
    echo "Test Failed: No Tests Passed"
    exit 2
fi