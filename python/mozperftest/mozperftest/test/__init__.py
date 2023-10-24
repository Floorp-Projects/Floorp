# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from mozperftest.layers import Layers
from mozperftest.test.androidlog import AndroidLog
from mozperftest.test.browsertime import BrowsertimeRunner
from mozperftest.test.mochitest import Mochitest
from mozperftest.test.webpagetest import WebPageTest
from mozperftest.test.xpcshell import XPCShell


def get_layers():
    return BrowsertimeRunner, AndroidLog, XPCShell, WebPageTest, Mochitest


def pick_test(env, flavor, mach_cmd):
    if flavor == "xpcshell":
        return Layers(env, mach_cmd, (XPCShell,))
    if flavor == "desktop-browser":
        return Layers(env, mach_cmd, (BrowsertimeRunner,))
    if flavor == "mobile-browser":
        return Layers(env, mach_cmd, (BrowsertimeRunner, AndroidLog))
    if flavor == "webpagetest":
        return Layers(env, mach_cmd, (WebPageTest,))
    if flavor == "mochitest":
        return Layers(env, mach_cmd, (Mochitest,))

    raise NotImplementedError(flavor)
