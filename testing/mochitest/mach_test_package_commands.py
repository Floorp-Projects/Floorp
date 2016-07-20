# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import unicode_literals

import os
from argparse import Namespace
from functools import partial

from mach.decorators import (
    CommandProvider,
    Command,
)

parser = None


def run_mochitest(context, **kwargs):
    args = Namespace(**kwargs)
    args.certPath = context.certs_dir
    args.utilityPath = context.bin_dir
    args.extraProfileFiles.append(os.path.join(context.bin_dir, 'plugins'))

    if not args.app:
        args.app = context.find_firefox()

    if args.test_paths:
        test_root = os.path.join(context.package_root, 'mochitest', 'tests')
        normalize = partial(context.normalize_test_path, test_root)
        args.test_paths = map(normalize, args.test_paths)

    from runtests import run_test_harness
    return run_test_harness(parser, args)


def setup_argument_parser():
    from mochitest_options import MochitestArgumentParser
    global parser
    parser = MochitestArgumentParser(app='generic')
    return parser


@CommandProvider
class MochitestCommands(object):

    def __init__(self, context):
        self.context = context

    @Command('mochitest', category='testing',
             description='Run the mochitest harness.',
             parser=setup_argument_parser)
    def mochitest(self, **kwargs):
        return run_mochitest(self.context, **kwargs)
