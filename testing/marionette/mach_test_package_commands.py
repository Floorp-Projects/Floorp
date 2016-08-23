# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import argparse
import os
import sys
from functools import partial

from mach.decorators import (
    CommandProvider,
    Command,
)

parser = None


def run_marionette(context, **kwargs):
    from marionette.runtests import (
        MarionetteTestRunner,
        MarionetteHarness
    )
    from mozlog.structured import commandline


    args = argparse.Namespace(**kwargs)

    if not args.binary:
        args.binary = context.find_firefox()

    test_root = os.path.join(context.package_root, 'marionette', 'tests')
    if not args.tests:
        args.tests = [os.path.join(test_root, 'testing', 'marionette', 'harness',
                                   'marionette', 'tests', 'unit-tests.ini')]

    normalize = partial(context.normalize_test_path, test_root)
    args.tests = map(normalize, args.tests)

    commandline.add_logging_group(parser)
    parser.verify_usage(args)

    args.logger = commandline.setup_logging("Marionette Unit Tests",
                                            args,
                                            {"mach": sys.stdout})
    status = MarionetteHarness(MarionetteTestRunner, args=vars(args)).run()
    return 1 if status else 0


def setup_marionette_argument_parser():
    from marionette.runner.base import BaseMarionetteArguments
    global parser
    parser = BaseMarionetteArguments()
    return parser


@CommandProvider
class MachCommands(object):

    def __init__(self, context):
        self.context = context

    @Command(
        'marionette-test', category='testing',
        description='Run a Marionette test (Check UI or the internal JavaScript '
                    'using marionette).',
        parser=setup_marionette_argument_parser)
    def run_marionette_test(self, **kwargs):
        return run_marionette(self.context, **kwargs)
