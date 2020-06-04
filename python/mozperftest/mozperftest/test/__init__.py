# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from mozperftest.test.browsertime import BrowsertimeRunner
from mozperftest.test.profile import Profile
from mozperftest.layers import Layers


def get_layers():
    return (Profile, BrowsertimeRunner)


def pick_test(env, flavor, mach_cmd):
    if flavor == "script":
        return Layers(env, mach_cmd, get_layers())
    raise NotImplementedError(flavor)
