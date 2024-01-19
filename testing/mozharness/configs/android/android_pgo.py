# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Mozharness configuration for Android PGO.
#
# This configuration should be combined with platform-specific mozharness
# configuration such as androidarm.py, or similar.

config = {
    "default_actions": [
        "download",
        "create-virtualenv",
        "start-emulator",
        "verify-device",
        "install",
        "run-tests",
    ],
}
