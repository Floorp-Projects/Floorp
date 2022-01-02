# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This config is for `mach try release` to support beta simulations.

config = {
    "replacements": [
        ("build/defines.sh", "EARLY_BETA_OR_EARLIER=1", "EARLY_BETA_OR_EARLIER="),
    ],
}
