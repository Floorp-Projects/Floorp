# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

config = {
    "stage_platform": "win64-st-an-debug",
    "debug_build": True,
    "env": {
        "XPCOM_DEBUG_BREAK": "stack-and-abort",
        # Disable sccache because otherwise we won't index the files that
        # sccache optimizes away compilation for
        "SCCACHE_DISABLE": "1",
    },
    "mozconfig_variant": "debug-searchfox",
}
