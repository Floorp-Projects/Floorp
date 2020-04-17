# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from argparse import ArgumentParser, SUPPRESS
import os
import mozlog
import copy

here = os.path.abspath(os.path.dirname(__file__))
try:
    from mozbuild.base import MozbuildObject, MachCommandConditions as conditions

    build_obj = MozbuildObject.from_environment(cwd=here)
except ImportError:
    build_obj = None
    conditions = None

from mozperftest.system import get_layers as system_layers
from mozperftest.browser import get_layers as browser_layers
from mozperftest.metrics import get_layers as metrics_layers


class Options:

    general_args = {
        "tests": {
            "nargs": "*",
            "metavar": "TEST",
            "default": [],
            "help": "Test to run. Can be a single test file or a directory of tests "
            "(to run recursively). If omitted, the entire suite is run.",
        },
        "--extra-options": {
            "type": str,
            "default": "",
            "help": "Extra options passed through",
        },
        "--output": {
            "type": str,
            "default": "artifacts",
            "help": "Path to where data will be stored, defaults to a top-level "
            "`artifacts` folder.",
        },
        "--cycles": {"type": int, "default": 1, "help": "Number of full cycles"},
        "--hooks": {"type": str, "default": "", "help": "Python hooks"},
        "--verbose": {"action": "store_true", "default": False, "help": "Verbose mode"},
    }

    args = copy.deepcopy(general_args)


for layer in system_layers() + browser_layers() + metrics_layers():
    for option, value in layer.arguments.items():
        if option in Options.args:
            raise KeyError("%s option already defined!" % option)
        Options.args[option] = value


class PerftestArgumentParser(ArgumentParser):
    """%(prog)s [options] [test paths]"""

    def __init__(self, app=None, **kwargs):
        ArgumentParser.__init__(
            self, usage=self.__doc__, conflict_handler="resolve", **kwargs
        )
        # XXX see if this list will vary depending on the flavor & app
        self.oldcwd = os.getcwd()
        self.app = app
        if not self.app and build_obj:
            if conditions.is_android(build_obj):
                self.app = "android"
        if not self.app:
            self.app = "generic"
        for name, options in Options.args.items():
            if "default" in options and isinstance(options["default"], list):
                options["default"] = []
            if "suppress" in options:
                if options["suppress"]:
                    options["help"] = SUPPRESS
                del options["suppress"]

            self.add_argument(name, **options)

        mozlog.commandline.add_logging_group(self)
