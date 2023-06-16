# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

config = {
    "stage_platform": "android-arm-gradle-dependencies",
    "src_mozconfig": "mobile/android/config/mozconfigs/android-arm-gradle-dependencies/nightly",
    # gradle-dependencies doesn't produce a package. So don't collect package metrics.
    "postflight_build_mach_commands": [
        [
            "android",
            "gradle-dependencies",
        ],
    ],
}
