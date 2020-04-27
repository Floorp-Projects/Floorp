# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from mozperftest.layers import Layers
from mozperftest.metrics.perfherder import Perfherder
from mozperftest.metrics.consoleoutput import ConsoleOutput


def get_layers():
    return Perfherder, ConsoleOutput


def pick_metrics(env, flavor, mach_cmd):
    if flavor == "script":
        return Layers(env, mach_cmd, get_layers())
    raise NotImplementedError(flavor)
