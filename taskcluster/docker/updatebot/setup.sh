#!/bin/bash
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

set -vex

whoami

# If poetry is not run as worker, then it won't work when run as user later.
cd /builds/worker/updatebot
/usr/local/bin/poetry install
