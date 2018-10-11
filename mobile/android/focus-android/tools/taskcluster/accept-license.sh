#!/usr/bin/env bash
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# If a command fails, do not fail the script
set -ex

# Accept SDK license
if [[ $(type -P "sdkmanager") ]]; then
    yes | sdkmanager "platforms;android-28" "build-tools;28.0.3"
elif [[ $(type -P "android") ]]; then
    (while sleep 3; do echo "y"; done) | android update sdk --no-ui --all --filter android-28
    (while sleep 3; do echo "y"; done) | android update sdk --no-ui --all --filter build-tools-28.0.3
fi
