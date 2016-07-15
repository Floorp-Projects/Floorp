# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import unicode_literals

import os
import sys
from argparse import Namespace
from functools import partial


import mozlog
from xpcshellcommandline import parser_desktop

from mach.decorators import (
    CommandProvider,
    Command,
)


def run_xpcshell(context, **kwargs):
    args = Namespace(**kwargs)
    args.utility_path = context.bin_dir
    args.testingModulesDir = context.modules_dir

    if not args.appPath:
        args.appPath = os.path.dirname(context.find_firefox())

    if not args.xpcshell:
        args.xpcshell = os.path.join(args.appPath, 'xpcshell')

    if not args.pluginsPath:
        for path in context.ancestors(args.appPath, depth=2):
            test = os.path.join(path, 'plugins')
            if os.path.isdir(test):
                args.pluginsPath = test
                break

    log = mozlog.commandline.setup_logging("XPCShellTests",
                                           args,
                                           {"mach": sys.stdout},
                                           {"verbose": True})

    if args.testPaths:
        test_root = os.path.join(context.package_root, 'xpcshell', 'tests')
        normalize = partial(context.normalize_test_path, test_root)
        args.testPaths = map(normalize, args.testPaths)

    import runxpcshelltests
    xpcshell = runxpcshelltests.XPCShellTests(log=log)
    return xpcshell.runTests(**vars(args))


@CommandProvider
class MochitestCommands(object):

    def __init__(self, context):
        self.context = context

    @Command('xpcshell-test', category='testing',
             description='Run the xpcshell harness.',
             parser=parser_desktop)
    def xpcshell(self, **kwargs):
        return run_xpcshell(self.context, **kwargs)
