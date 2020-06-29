# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# If a command fails then do not proceed and fail this script too.
set -ex

URL="https://firefoxci.taskcluster-artifacts.net/$TASK_ID/0/public/reports"

echo "#### Test result files
_(404 if task didn't run)_

- [Unit Tests (Debug)]($URL/tests/testDebugUnitTest/index.html)
- [Unit Tests (Release)]($URL/tests/testReleaseUnitTest/index.html)
- [Android Lint]($URL/lint-results-release.html)" > /builds/worker/github/customCheckRunText.md
