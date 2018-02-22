#!/usr/bin/env bash
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This script imports the latest strings, creates a commit, pushes
# it to the bot's repository and creates a pull request.

# If a command fails then do not proceed and fail this script too.
set -ex

# download the login JSON token info from taskcluster
# (Commented out since secrets need to be added into TC in the right place)
python tools/taskcluster/get-google-firebase-token.py

# this is where the Google Testcloud project ID is set
/google-cloud-sdk/bin/gcloud config set project moz-fx-mobile-firebase-testlab

 # this is where the service account login happens with the saved JSON from above
/google-cloud-sdk/bin/gcloud auth activate-service-account --key-file .firebase_token.json