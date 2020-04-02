# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from mozperftest.base import MultipleMachEnvironment
from mozperftest.system.proxy import ProxyRunner


def pick_system(flavor, mach_cmd):
    if flavor == "script":
        return MultipleMachEnvironment(mach_cmd, (ProxyRunner,))
    raise NotImplementedError(flavor)
