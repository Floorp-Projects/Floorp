# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import unicode_literals
import os

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

def run_marionette(tests, b2g_path=None, emulator=None, testtype=None,
    address=None, bin=None, topsrcdir=None):
    from marionette.runtests import (
        MarionetteTestRunner,
        BaseMarionetteOptions,
        startTestRunner
    )

    parser = BaseMarionetteOptions()
    options, args = parser.parse_args()

    if not tests:
        tests = [os.path.join(topsrcdir,
                    'testing/marionette/client/marionette/tests/unit-tests.ini')]

    options.type = testtype
    if b2g_path:
        options.homedir = b2g_path
        if emulator:
            options.emulator = emulator
    else:
        options.bin = bin
        path, exe = os.path.split(options.bin)

    options.address = address

    parser.verify_usage(options, tests)

    runner = startTestRunner(MarionetteTestRunner, options, tests)
    if runner.failed > 0:
        return 1

    return 0

@CommandProvider
class B2GCommands(MachCommandBase):
    def __init__(self, context):
        MachCommandBase.__init__(self, context)

        for attr in ('b2g_home', 'device_name'):
            setattr(self, attr, getattr(context, attr, None))
    @Command('marionette-webapi', category='testing',
        description='Run a Marionette webapi test',
        conditions=[conditions.is_b2g])
    @CommandArgument('--emulator', choices=['x86', 'arm'],
        help='Run an emulator of the specified architecture.')
    @CommandArgument('--type', dest='testtype',
        help='Test type, usually one of: browser, b2g, b2g-qemu.',
        default='b2g')
    @CommandArgument('tests', nargs='*', metavar='TESTS',
        help='Path to test(s) to run.')
    def run_marionette_webapi(self, tests, emulator=None, testtype=None):
        if not emulator and self.device_name.find('emulator') == 0:
            emulator='arm'

        if self.substs.get('ENABLE_MARIONETTE') != '1':
            print(MARIONETTE_DISABLED_B2G % 'marionette-webapi')
            return 1

        return run_marionette(tests, b2g_path=self.b2g_home, emulator=emulator,
            testtype=testtype, topsrcdir=self.topsrcdir, address=None)


@CommandProvider
class MachCommands(MachCommandBase):
    @Command('marionette-test', category='testing',
        description='Run a Marionette test.',
        conditions=[conditions.is_firefox])
    @CommandArgument('--address',
        help='host:port of running Gecko instance to connect to.')
    @CommandArgument('--type', dest='testtype',
        help='Test type, usually one of: browser, b2g, b2g-qemu.',
        default='browser')
    @CommandArgument('tests', nargs='*', metavar='TESTS',
        help='Path to test(s) to run.')
    def run_marionette_test(self, tests, address=None, testtype=None):
        bin = self.get_binary_path('app')
        return run_marionette(tests, bin=bin, testtype=testtype,
            topsrcdir=self.topsrcdir, address=address)
