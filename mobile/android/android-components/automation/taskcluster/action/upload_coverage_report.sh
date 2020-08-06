# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# If a command fails then do not proceed and fail this script too.
set -ex

# Set some environment variables that will help codecov detect the CI
export CI_BUILD_URL="https://tools.taskcluster.net/tasks/$TASK_ID"

# Execute codecov script for uploading report
# bash <(curl -s https://codecov.io/bash)
# Copied the codecov bash script locally for:
# https://github.com/mozilla-mobile/android-components/issues/7500
bash automation/taskcluster/.codecov -t @.cc_token
