# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os

config = {
    "package-name": "firefox",
    "installer-tag": "browser/installer/windows/app.tag",
    "stub-installer-tag": "browser/installer/windows/stub.tag",
    "wsx-stub": "browser/installer/windows/msi/installer.wxs",
    "fetch-dir": os.environ.get("MOZ_FETCHES_DIR"),
}
