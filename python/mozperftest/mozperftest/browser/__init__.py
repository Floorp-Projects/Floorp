# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from mozperftest.browser.browsertime import BrowsertimeRunner


def pick_browser(flavor, mach_cmd):
    if flavor == "script":
        return BrowsertimeRunner(mach_cmd)
    raise NotImplementedError(flavor)
