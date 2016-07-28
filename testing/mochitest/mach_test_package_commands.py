# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import unicode_literals

from argparse import Namespace
import os

from mach.decorators import (
    CommandProvider,
    Command,
)


def run_mochitest(context, **kwargs):
    args = Namespace(**kwargs)
    args.certPath = context.certs_dir
    args.utilityPath = context.bin_dir
    args.extraProfileFiles.append(os.path.join(context.bin_dir, 'plugins'))

    from runtests import run_test_harness
    return run_test_harness(args)


def setup_argument_parser():
    from mochitest_options import MochitestArgumentParser
    return MochitestArgumentParser(app='generic')


@CommandProvider
class MochitestCommands(object):

    def __init__(self, context):
        self.context = context

    @Command('mochitest', category='testing',
             description='Run the mochitest harness.',
             parser=setup_argument_parser)
    def mochitest(self, **kwargs):
        return run_mochitest(self.context, **kwargs)
