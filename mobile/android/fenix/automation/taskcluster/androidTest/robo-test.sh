#!/usr/bin/env bash
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This script does the following:
# 1. Retrieves gcloud service account token
# 2. Activates gcloud service account
# 3. Connects to Google's Firebase Test Lab (using Flank)
# 4. Executes Robo test
# 5. Puts any artifacts into the test_artifacts folder

# If a command fails then do not proceed and fail this script too.
set -e

get_abs_filename() {
  relative_filename="$1"
  echo "$(cd "$(dirname "$relative_filename")" && pwd)/$(basename "$relative_filename")"
}

# Basic parameter check
if [[ $# -lt 1 ]]; then
    echo "Error: please provide a Flank configuration"
    display_help
    exit 1
fi

device_type="$1" # flank-arm-robo-test.yml | flank-x86-robo-test.yml
APK_APP="$2"
JAVA_BIN="/usr/bin/java"
PATH_TEST="./automation/taskcluster/androidTest"
FLANK_BIN="/builds/worker/test-tools/flank.jar"
ARTIFACT_DIR="/builds/worker/artifacts"
RESULTS_DIR="${ARTIFACT_DIR}/results"

echo
echo "ACTIVATE SERVICE ACCT"
echo
gcloud config set project "$GOOGLE_PROJECT"
gcloud auth activate-service-account --key-file "$GOOGLE_APPLICATION_CREDENTIALS"
echo
echo
echo

set +e

flank_template="${PATH_TEST}/flank-${device_type}.yml"
if [ -f "$flank_template" ]; then
    echo "Using Flank template: $flank_template"
else
    echo "Error: Flank template not found: $flank_template"
    exit 1
fi

APK_APP="$(get_abs_filename $APK_APP)"

function failure_check() {
    echo
    echo
    if [[ $exitcode -ne 0 ]]; then
        echo "FAILURE: Robo test run failed, please check above URL"
    else
	    echo "Robo test was successful!"
    fi

    echo
    echo "RESULTS"
    echo

    mkdir -p /builds/worker/artifacts/github
    chmod +x $PATH_TEST/parse-ui-test.py
    $PATH_TEST/parse-ui-test.py \
        --exit-code "${exitcode}" \
        --log flank.log \
        --results "${RESULTS_DIR}" \
        --output-md "${ARTIFACT_DIR}/github/customCheckRunText.md" \
	    --device-type "${device_type}"
}

echo
echo "FLANK VERSION"
echo
$JAVA_BIN -jar $FLANK_BIN --version
echo
echo

echo
echo "EXECUTE ROBO TEST"
echo
set -o pipefail && $JAVA_BIN -jar $FLANK_BIN android run \
	--config=$flank_template \
	--app=$APK_APP \
	--local-result-dir="${RESULTS_DIR}" \
	--project=$GOOGLE_PROJECT \
	--client-details=commit=${MOBILE_HEAD_REV:-None},pullRequest=${PULL_REQUEST_NUMBER:-None} \
	| tee flank.log

exitcode=$?
failure_check

exit $exitcode
