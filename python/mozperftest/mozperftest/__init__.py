# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import mozlog

from mozperftest.browser import pick_browser
from mozperftest.system import pick_system
from mozperftest.metrics import pick_metrics
from mozperftest.argparser import PerftestArgumentParser


logger = mozlog.commandline.setup_logging("mozperftest", {})


def get_parser():
    return PerftestArgumentParser()


# XXX todo, turn the dict into a class that controls
# what gets in and out of it
# we want a structure with :
#  - results
#  - browser prefs
#  - ??
def get_metadata(mach_cmd, flavor, **kwargs):
    res = {
        "mach_cmd": mach_cmd,
        "mach_args": kwargs,
        "flavor": flavor,
        "browser": {"prefs": {}},
    }
    return res


def get_env(mach_cmd, flavor="script", test_objects=None, resolve_tests=True, **kwargs):
    # XXX do something with flavors, etc
    if flavor != "script":
        raise NotImplementedError(flavor)

    return [
        layer(flavor, mach_cmd) for layer in (pick_system, pick_browser, pick_metrics)
    ]
