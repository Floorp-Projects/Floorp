# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from mozperftest.layers import Layers
from mozperftest.metrics.consoleoutput import ConsoleOutput
from mozperftest.metrics.notebookupload import Notebook
from mozperftest.metrics.perfboard.influx import Influx
from mozperftest.metrics.perfherder import Perfherder
from mozperftest.metrics.visualmetrics import VisualMetrics


def get_layers():
    return VisualMetrics, Perfherder, ConsoleOutput, Notebook, Influx


def pick_metrics(env, flavor, mach_cmd):
    if flavor in ("desktop-browser", "mobile-browser"):
        layers = get_layers()
    else:
        # we don't need VisualMetrics for xpcshell
        layers = Perfherder, ConsoleOutput, Notebook, Influx

    return Layers(env, mach_cmd, layers)
