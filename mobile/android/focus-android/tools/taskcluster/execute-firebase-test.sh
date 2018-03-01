#!/usr/bin/env bash
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This script installs google cloud sdk, logs into google firebase, executes tests,
# and collects test artifacts into the test_artifacts folder

# If a command fails then do not proceed and fail this script too.
set -ex

./tools/taskcluster/google-firebase-testlab-login.sh

# From now on disable exiting on error. If the tests fail we want to continue
# and try to download the artifacts. We will exit with the actual error code later.
set +e

# Execute test set
/google-cloud-sdk/bin/gcloud --format="json" firebase test android run \
--type=instrumentation \
--app="app/build/outputs/apk/focusWebviewUniversal/debug/app-focus-webview-universal-debug.apk" \
--test="app/build/outputs/apk/androidTest/focusWebviewUniversal/debug/app-focus-webview-universal-debug-androidTest.apk" \
--results-bucket="focus_android_test_artifacts" \
--timeout="30m" \
--no-auto-google-login \
--test-runner-class="android.support.test.runner.AndroidJUnitRunner" \
--device="model=sailfish,version=26" \
--device="model=Nexus5X,version=23" \
--device="model=Nexus9,version=25" \
--device="model=sailfish,version=25"

exitcode=$?

echo "Downloading artifacts"

mkdir test_artifacts
/google-cloud-sdk/bin/gsutil ls gs://focus_android_test_artifacts | tail -1 | /google-cloud-sdk/bin/gsutil -m cp -r -I ./test_artifacts

# Now exit the script with the exit code from the test run. (Only 0 if all test executions passed)
exit $exitcode
