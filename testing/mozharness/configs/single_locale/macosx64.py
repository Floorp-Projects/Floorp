# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

config = {
    "mozconfig_platform": "macosx64",
    "upload_env_extra": {
        "MOZ_PKG_PLATFORM": "mac",
    },
    # l10n
    "ignore_locales": ["en-US", "ja"],
}
