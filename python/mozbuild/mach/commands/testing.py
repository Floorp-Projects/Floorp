# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import unicode_literals

from mach.base import CommandArgument
from mach.base import CommandProvider
from mach.base import Command
from mozbuild.base import MozbuildObject


generic_help = 'Test to run. Can be specified as a single JS file, a ' +\
'directory, or omitted. If omitted, the entire test suite is executed.'

suites = ['mochitest-plain', 'mochitest-chrome', 'mochitest-browser', 'all']


@CommandProvider
class Testing(MozbuildObject):
    """Provides commands for running tests."""

    @Command('test', help='Perform tests.')
    @CommandArgument('suite', default='all', choices=suites, nargs='?',
        help='Test suite to run.')
    def run_suite(self, suite):
        from mozbuild.testing.suite import Suite

        s = self._spawn(Suite)
        s.run_suite(suite)

    @Command('mochitest-plain', help='Run a plain mochitest.')
    @CommandArgument('test_file', default=None, nargs='?', metavar='TEST',
        help=generic_help)
    def run_mochitest_plain(self, test_file):
        self.run_mochitest(test_file, 'plain')

    @Command('mochitest-chrome', help='Run a chrome mochitest.')
    @CommandArgument('test_file', default=None, nargs='?', metavar='TEST',
        help=generic_help)
    def run_mochitest_chrome(self, test_file):
        self.run_mochitest(test_file, 'chrome')

    @Command('mochitest-browser', help='Run a mochitest with browser chrome.')
    @CommandArgument('test_file', default=None, nargs='?', metavar='TEST',
        help=generic_help)
    def run_mochitest_browser(self, test_file):
        self.run_mochitest(test_file, 'browser')

    @Command('mochitest-a11y', help='Run an a11y mochitest.')
    @CommandArgument('test_file', default=None, nargs='?', metavar='TEST',
        help=generic_help)
    def run_mochitest_a11y(self, test_file):
        self.run_mochitest(test_file, 'a11y')

    def run_mochitest(self, test_file, flavor):
        from mozbuild.testing.mochitest import MochitestRunner

        mochitest = self._spawn(MochitestRunner)
        mochitest.run_mochitest_test(test_file, flavor)

    @Command('reftest', help='Run a reftest.')
    @CommandArgument('test_file', default=None, nargs='?', metavar='TEST',
        help=generic_help)
    def run_reftest(self, test_file):
        self._run_reftest(test_file, 'reftest')

    @Command('crashtest', help='Run a crashtest.')
    @CommandArgument('test_file', default=None, nargs='?', metavar='TEST',
        help=generic_help)
    def run_crashtest(self, test_file):
        self._run_reftest(test_file, 'crashtest')

    def _run_reftest(self, test_file, flavor):
        from mozbuild.testing.reftest import ReftestRunner

        reftest = self._spawn(ReftestRunner)
        reftest.run_reftest_test(test_file, flavor)

    @Command('xpcshell-test', help='Run an xpcshell test.')
    @CommandArgument('test_file', default='all', nargs='?', metavar='TEST',
        help=generic_help)
    @CommandArgument('--debug', '-d', action='store_true',
        help='Run test in a debugger.')
    def run_xpcshell_test(self, **params):
        from mozbuild.testing.xpcshell import XPCShellRunner

        xpcshell = self._spawn(XPCShellRunner)
        xpcshell.run_test(**params)

