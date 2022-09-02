# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from argparse import ArgumentParser, Namespace
import os
import mozlog
import copy

here = os.path.abspath(os.path.dirname(__file__))
try:
    from mozbuild.base import MozbuildObject, MachCommandConditions as conditions

    build_obj = MozbuildObject.from_environment(cwd=here)
except Exception:
    build_obj = None
    conditions = None

from mozperftest.system import get_layers as system_layers  # noqa
from mozperftest.test import get_layers as test_layers  # noqa
from mozperftest.metrics import get_layers as metrics_layers  # noqa
from mozperftest.utils import convert_day  # noqa

FLAVORS = "desktop-browser", "mobile-browser", "doc", "xpcshell", "webpagetest"


class Options:

    general_args = {
        "--flavor": {
            "choices": FLAVORS,
            "metavar": "{{{}}}".format(", ".join(FLAVORS)),
            "default": "desktop-browser",
            "help": "Only run tests of this flavor.",
        },
        "tests": {
            "nargs": "*",
            "metavar": "TEST",
            "default": [],
            "help": "Test to run. Can be a single test file or URL or a directory"
            " of tests (to run recursively). If omitted, the entire suite is run.",
        },
        "--test-iterations": {
            "type": int,
            "default": 1,
            "help": "Number of times the whole test is executed",
        },
        "--output": {
            "type": str,
            "default": "artifacts",
            "help": "Path to where data will be stored, defaults to a top-level "
            "`artifacts` folder.",
        },
        "--hooks": {
            "type": str,
            "default": None,
            "help": "Script containing hooks. Can be a path or a URL.",
        },
        "--verbose": {"action": "store_true", "default": False, "help": "Verbose mode"},
        "--push-to-try": {
            "action": "store_true",
            "default": False,
            "help": "Pushin the test to try",
        },
        "--try-platform": {
            "nargs": "*",
            "type": str,
            "default": "linux",
            "help": "Platform to use on try",
            "choices": ["g5", "pixel2", "linux", "mac", "win"],
        },
        "--on-try": {
            "action": "store_true",
            "default": False,
            "help": "Running the test on try",
        },
        "--test-date": {
            "type": convert_day,
            "default": "today",
            "help": "Used in multi-commit testing, it specifies the day to get test builds from. "
            "Must follow the format `YYYY.MM.DD` or be `today` or `yesterday`.",
        },
    }

    args = copy.deepcopy(general_args)


for layer in system_layers() + test_layers() + metrics_layers():
    if layer.activated:
        # add an option to deactivate it
        option_name = "--no-%s" % layer.name
        option_help = "Deactivates the %s layer" % layer.name
    else:
        option_name = "--%s" % layer.name
        option_help = "Activates the %s layer" % layer.name

    Options.args[option_name] = {
        "action": "store_true",
        "default": False,
        "help": option_help,
    }

    for option, value in layer.arguments.items():
        option = "--%s-%s" % (layer.name, option.replace("_", "-"))
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
            self.add_argument(name, **options)

        mozlog.commandline.add_logging_group(self)
        self.set_by_user = []

    def parse_helper(self, args):
        for arg in args:
            arg_part = arg.partition("--")[-1].partition("-")
            layer_name = f"--{arg_part[0]}"
            layer_exists = arg_part[1] and layer_name in Options.args
            if layer_exists:
                args.append(layer_name)

    def get_user_args(self, args):
        # suppress args that were not provided by the user.
        res = {}
        for key, value in args.items():
            if key not in self.set_by_user:
                continue
            res[key] = value
        return res

    def _parse_known_args(self, arg_strings, namespace):
        # at this point, the namespace is filled with default values
        # defined in the args

        # let's parse what the user really gave us in the CLI
        # in a new namespace
        user_namespace, extras = super()._parse_known_args(arg_strings, Namespace())

        self.set_by_user = list([name for name, value in user_namespace._get_kwargs()])

        # we can now merge both
        for key, value in user_namespace._get_kwargs():
            setattr(namespace, key, value)

        return namespace, extras

    def parse_args(self, args=None, namespace=None):
        self.parse_helper(args)
        return super().parse_args(args, namespace)

    def parse_known_args(self, args=None, namespace=None):
        self.parse_helper(args)
        return super().parse_known_args(args, namespace)


class SideBySideOptions:
    args = [
        [
            ["-t", "--test-name"],
            {
                "type": str,
                "required": True,
                "dest": "test_name",
                "help": "The name of the test task to get videos from.",
            },
        ],
        [
            ["--new-test-name"],
            {
                "type": str,
                "default": None,
                "help": "The name of the test task to get videos from in the new revision.",
            },
        ],
        [
            ["--base-revision"],
            {
                "type": str,
                "required": True,
                "help": "The base revision to compare a new revision to.",
            },
        ],
        [
            ["--new-revision"],
            {
                "type": str,
                "required": True,
                "help": "The base revision to compare a new revision to.",
            },
        ],
        [
            ["--base-branch"],
            {
                "type": str,
                "default": "autoland",
                "help": "Branch to search for the base revision.",
            },
        ],
        [
            ["--new-branch"],
            {
                "type": str,
                "default": "autoland",
                "help": "Branch to search for the new revision.",
            },
        ],
        [
            ["--base-platform"],
            {
                "type": str,
                "required": True,
                "dest": "platform",
                "help": "Platform to return results for.",
            },
        ],
        [
            ["--new-platform"],
            {
                "type": str,
                "default": None,
                "help": "Platform to return results for in the new revision.",
            },
        ],
        [
            ["-o", "--overwrite"],
            {
                "action": "store_true",
                "default": False,
                "help": "If set, the downloaded task group data will be deleted before "
                + "it gets re-downloaded.",
            },
        ],
        [
            ["--cold"],
            {
                "action": "store_true",
                "default": False,
                "help": "If set, we'll only look at cold pageload tests.",
            },
        ],
        [
            ["--warm"],
            {
                "action": "store_true",
                "default": False,
                "help": "If set, we'll only look at warm pageload tests.",
            },
        ],
        [
            ["--most-similar"],
            {
                "action": "store_true",
                "default": False,
                "help": "If set, we'll search for a video pairing that is the most similar.",
            },
        ],
        [
            ["--search-crons"],
            {
                "action": "store_true",
                "default": False,
                "help": "If set, we will search for the tasks within the cron jobs as well. ",
            },
        ],
        [
            ["--skip-download"],
            {
                "action": "store_true",
                "default": False,
                "help": "If set, we won't try to download artifacts again and we'll "
                + "try using what already exists in the output folder.",
            },
        ],
        [
            ["--output"],
            {
                "type": str,
                "default": None,
                "help": "This is where the data will be saved. Defaults to CWD. "
                + "You can include a name for the file here, otherwise it will "
                + "default to side-by-side.mp4.",
            },
        ],
        [
            ["--metric"],
            {
                "type": str,
                "default": "speedindex",
                "help": "Metric to use for side-by-side comparison.",
            },
        ],
        [
            ["--vismetPath"],
            {
                "type": str,
                "default": False,
                "help": "Paths to visualmetrics.py for step chart generation.",
            },
        ],
        [
            ["--original"],
            {
                "action": "store_true",
                "default": False,
                "help": "If set, use the original videos in the side-by-side instead "
                + "of the postprocessed videos.",
            },
        ],
        [
            ["--skip-slow-gif"],
            {
                "action": "store_true",
                "default": False,
                "help": "If set, the slow-motion GIFs won't be produced.",
            },
        ],
    ]


class ToolingOptions:
    args = {
        "side-by-side": SideBySideOptions.args,
    }


class PerftestToolsArgumentParser(ArgumentParser):
    """%(prog)s [options] [test paths]"""

    tool = None

    def __init__(self, *args, **kwargs):
        ArgumentParser.__init__(
            self, usage=self.__doc__, conflict_handler="resolve", **kwargs
        )

        if PerftestToolsArgumentParser.tool is None:
            raise SystemExit("No tool specified, cannot continue parsing")
        else:
            for name, options in ToolingOptions.args[PerftestToolsArgumentParser.tool]:
                self.add_argument(*name, **options)
