# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

config = {
    "default_actions": [
        "build",
    ],
    "stage_platform": "win64-rusttests",
    "env": {
        "XPCOM_DEBUG_BREAK": "stack-and-abort",
    },
    "app_name": "tools/rusttests",
    "disable_package_metrics": True,
}
