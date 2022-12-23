# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import sys
from argparse import Namespace
from functools import partial

import mozlog
from mach.decorators import Command
from xpcshellcommandline import parser_desktop


def run_xpcshell(context, **kwargs):
    args = Namespace(**kwargs)
    args.appPath = args.appPath or os.path.dirname(context.firefox_bin)
    args.utility_path = context.bin_dir
    args.testingModulesDir = context.modules_dir

    if not args.xpcshell:
        args.xpcshell = os.path.join(args.appPath, "xpcshell")

    log = mozlog.commandline.setup_logging(
        "XPCShellTests", args, {"mach": sys.stdout}, {"verbose": True}
    )

    if args.testPaths:
        test_root = os.path.join(context.package_root, "xpcshell", "tests")
        normalize = partial(context.normalize_test_path, test_root)
        # pylint --py3k: W1636
        args.testPaths = list(map(normalize, args.testPaths))

    import runxpcshelltests

    xpcshell = runxpcshelltests.XPCShellTests(log=log)
    return xpcshell.runTests(**vars(args))


@Command(
    "xpcshell-test",
    category="testing",
    description="Run the xpcshell harness.",
    parser=parser_desktop,
)
def xpcshell(command_context, **kwargs):
    command_context._mach_context.activate_mozharness_venv()
    return run_xpcshell(command_context._mach_context, **kwargs)
