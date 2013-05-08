# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import unicode_literals
import os

from mozbuild.base import MachCommandBase

from mach.decorators import (
    CommandArgument,
    CommandProvider,
    Command,
)


@CommandProvider
class MachCommands(MachCommandBase):
    @Command('marionette-test', help='Run a Marionette test.')
    @CommandArgument('--homedir', dest='b2g_path',
        help='For B2G testing, the path to the B2G repo.')
    @CommandArgument('--emulator', choices=['x86', 'arm'],
        help='Run an emulator of the specified architecture.')
    @CommandArgument('--address',
        help='host:port of running Gecko instance to connect to.')
    @CommandArgument('--type', dest='testtype',
        help='Test type, usually one of: browser, b2g, b2g-qemu.')
    @CommandArgument('tests', nargs='*', metavar='TESTS',
        help='Path to test(s) to run.')
    def run_marionette(self, tests, emulator=None, address=None, b2g_path=None,
            testtype=None):
        from marionette.runtests import (
            MarionetteTestRunner,
            MarionetteTestOptions,
            startTestRunner
        )

        parser = MarionetteTestOptions()
        options, args = parser.parse_args()

        if not tests:
            tests = ['testing/marionette/client/marionette/tests/unit-tests.ini']

        options.type = testtype
        if emulator:
            if b2g_path:
                options.homedir = b2g_path
            if not testtype:
                options.type = "b2g"
        else:
            if not testtype:
                options.type = "browser"
            try:
                bin = self.get_binary_path('app')
                options.bin = bin
            except Exception as e:
                print("It looks like your program isn't built.",
                      "You can run |mach build| to build it.")
                print(e)
                return 1
            path, exe = os.path.split(options.bin)
            if 'b2g' in exe:
                options.app = 'b2gdesktop'

        if not emulator:
            if self.substs.get('ENABLE_MARIONETTE') != '1':
                print("Marionette doesn't appear to be enabled; please "
                      "add ENABLE_MARIONETTE=1 to your mozconfig and "
                      "perform a clobber build.")
                return 1

        options.address = address

        parser.verify_usage(options, tests)

        runner = startTestRunner(MarionetteTestRunner, options, tests)
        if runner.failed > 0:
            return 1

        return 0
