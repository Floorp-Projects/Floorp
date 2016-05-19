# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, unicode_literals

import os
import sys
import argparse

from mozbuild.base import (
    MachCommandBase,
    MachCommandConditions as conditions,
)

from mach.decorators import (
    CommandArgument,
    CommandProvider,
    Command,
)

MARIONETTE_DISABLED_B2G = '''
The %s command requires a Marionette-enabled build.

Please create an engineering build, which has Marionette enabled.  You can do
this by ommitting the VARIANT variable when building, or using:

VARIANT=eng ./build.sh
'''

def setup_argument_parser():
    from marionette.runner.base import BaseMarionetteArguments
    return BaseMarionetteArguments()

def run_marionette(tests, b2g_path=None, emulator=None,
    address=None, binary=None, topsrcdir=None, **kwargs):
    from mozlog.structured import commandline

    from marionette.runtests import (
        MarionetteTestRunner,
        BaseMarionetteArguments,
        MarionetteHarness
    )

    parser = BaseMarionetteArguments()
    commandline.add_logging_group(parser)

    if not tests:
        tests = [os.path.join(topsrcdir,
                 'testing/marionette/harness/marionette/tests/unit-tests.ini')]

    args = parser.parse_args(args=tests)

    if b2g_path:
        args.homedir = b2g_path
        if emulator:
            args.emulator = emulator
    else:
        args.binary = binary
        path, exe = os.path.split(args.binary)

    for k, v in kwargs.iteritems():
        setattr(args, k, v)

    parser.verify_usage(args)

    args.logger = commandline.setup_logging("Marionette Unit Tests",
                                            args,
                                            {"mach": sys.stdout})
    failed = MarionetteHarness(MarionetteTestRunner, args=vars(args)).run()
    if failed > 0:
        return 1
    else:
        return 0

@CommandProvider
class B2GCommands(MachCommandBase):
    def __init__(self, context):
        MachCommandBase.__init__(self, context)

        for attr in ('b2g_home', 'device_name'):
            setattr(self, attr, getattr(context, attr, None))
    @Command('marionette-webapi', category='testing',
        description='Run a Marionette webapi test (test WebAPIs using marionette).',
        conditions=[conditions.is_b2g])
    @CommandArgument('--tag', action='append', dest='test_tags',
        help='Filter out tests that don\'t have the given tag. Can be used '
             'multiple times in which case the test must contain at least one '
             'of the given tags.')
    @CommandArgument('tests', nargs='*', metavar='TESTS',
        help='Path to test(s) to run.')
    def run_marionette_webapi(self, tests, **kwargs):
        emulator = None
        if self.device_name:
            if self.device_name.startswith('emulator'):
                emulator = 'arm'
                if 'x86' in self.device_name:
                    emulator = 'x86'

        if self.substs.get('ENABLE_MARIONETTE') != '1':
            print(MARIONETTE_DISABLED_B2G % 'marionette-webapi')
            return 1

        return run_marionette(tests, b2g_path=self.b2g_home, emulator=emulator,
            topsrcdir=self.topsrcdir, **kwargs)

@CommandProvider
class MachCommands(MachCommandBase):
    @Command('marionette-test', category='testing',
        description='Run a Marionette test (Check UI or the internal JavaScript using marionette).',
        conditions=[conditions.is_firefox],
        parser=setup_argument_parser,
    )
    def run_marionette_test(self, tests, **kwargs):
        if 'test_objects' in kwargs:
            tests = []
            for obj in kwargs['test_objects']:
                tests.append(obj['file_relpath'])
            del kwargs['test_objects']

        kwargs['binary'] = self.get_binary_path('app')
        return run_marionette(tests, topsrcdir=self.topsrcdir, **kwargs)
