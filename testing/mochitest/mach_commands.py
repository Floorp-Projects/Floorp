# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import unicode_literals

import os
import platform

from mozbuild.base import (
    MachCommandBase,
    MozbuildObject,
)

from mach.decorators import (
    CommandArgument,
    CommandProvider,
    Command,
)


generic_help = 'Test to run. Can be specified as a single file, a ' +\
'directory, or omitted. If omitted, the entire test suite is executed.'

debugger_help = 'Debugger binary to run test in. Program name or path.'


class MochitestRunner(MozbuildObject):
    """Easily run mochitests.

    This currently contains just the basics for running mochitests. We may want
    to hook up result parsing, etc.
    """
    def run_plain_suite(self):
        """Runs all plain mochitests."""
        # TODO hook up Python harness runner.
        self._run_make(directory='.', target='mochitest-plain')

    def run_chrome_suite(self):
        """Runs all chrome mochitests."""
        # TODO hook up Python harness runner.
        self._run_make(directory='.', target='mochitest-chrome')

    def run_browser_chrome_suite(self):
        """Runs browser chrome mochitests."""
        # TODO hook up Python harness runner.
        self._run_make(directory='.', target='mochitest-browser-chrome')

    def run_metro_chrome_suite(self):
        """Runs Windows 8 Metro browser chrome mochitests."""
        # TODO hook up Python harness runner.
        self._run_make(directory='.', target='mochitest-metro-chrome')

    def run_all(self):
        self.run_plain_suite()
        self.run_chrome_suite()
        self.run_browser_chrome_suite()
        # Run Metro tests only on Windows 8 and higher
        if platform.system() is 'Windows':
            version = tuple(map(int, platform.version().split('.')))
            if version >= (6, 2):
                self.run_metro_chrome_suite()

    def run_mochitest_test(self, suite=None, test_file=None, debugger=None):
        """Runs a mochitest.

        test_file is a path to a test file. It can be a relative path from the
        top source directory, an absolute filename, or a directory containing
        test files.

        suite is the type of mochitest to run. It can be one of ('plain',
        'chrome', 'browser', 'metro', 'a11y').

        debugger is a program name or path to a binary (presumably a debugger)
        to run the test in. e.g. 'gdb'
        """

        # TODO hook up harness via native Python
        target = None
        if suite == 'plain':
            target = 'mochitest-plain'
        elif suite == 'chrome':
            target = 'mochitest-chrome'
        elif suite == 'browser':
            target = 'mochitest-browser-chrome'
        elif suite == 'metro':
            target = 'mochitest-metro-chrome'
        elif suite == 'a11y':
            target = 'mochitest-a11y'
        else:
            raise Exception('None or unrecognized mochitest suite type.')

        if test_file:
            path_arg = self._wrap_path_argument(test_file)
            if not os.path.exists(path_arg.srcdir_path()):
                raise Exception('No test file was found at %s.' % test_file)
            env = {'TEST_PATH': path_arg.relpath()}
        else:
            env = {}

        pass_thru = False

        if debugger:
            env[b'EXTRA_TEST_ARGS'] = '--debugger=%s' % debugger
            pass_thru = True

        return self._run_make(directory='.', target=target, append_env=env,
            ensure_exit_code=False, pass_thru=pass_thru)


@CommandProvider
class MachCommands(MachCommandBase):
    @Command('mochitest-plain', help='Run a plain mochitest.')
    @CommandArgument('--debugger', '-d', metavar='DEBUGGER',
        help=debugger_help)
    @CommandArgument('test_file', default=None, nargs='?', metavar='TEST',
        help=generic_help)
    def run_mochitest_plain(self, test_file, debugger=None):
        return self.run_mochitest(test_file, 'plain', debugger=debugger)

    @Command('mochitest-chrome', help='Run a chrome mochitest.')
    @CommandArgument('--debugger', '-d', metavar='DEBUGGER',
        help=debugger_help)
    @CommandArgument('test_file', default=None, nargs='?', metavar='TEST',
        help=generic_help)
    def run_mochitest_chrome(self, test_file, debugger=None):
        return self.run_mochitest(test_file, 'chrome', debugger=debugger)

    @Command('mochitest-browser', help='Run a mochitest with browser chrome.')
    @CommandArgument('--debugger', '-d', metavar='DEBUGGER',
        help=debugger_help)
    @CommandArgument('test_file', default=None, nargs='?', metavar='TEST',
        help=generic_help)
    def run_mochitest_browser(self, test_file, debugger=None):
        return self.run_mochitest(test_file, 'browser', debugger=debugger)

    @Command('mochitest-metro', help='Run a mochitest with metro browser chrome.')
    @CommandArgument('--debugger', '-d', metavar='DEBUGGER',
        help=debugger_help)
    @CommandArgument('test_file', default=None, nargs='?', metavar='TEST',
        help=generic_help)
    def run_mochitest_metro(self, test_file, debugger=None):
        return self.run_mochitest(test_file, 'metro', debugger=debugger)

    @Command('mochitest-a11y', help='Run an a11y mochitest.')
    @CommandArgument('--debugger', '-d', metavar='DEBUGGER',
        help=debugger_help)
    @CommandArgument('test_file', default=None, nargs='?', metavar='TEST',
        help=generic_help)
    def run_mochitest_a11y(self, test_file, debugger=None):
        return self.run_mochitest(test_file, 'a11y', debugger=debugger)

    def run_mochitest(self, test_file, flavor, debugger=None):
        self._ensure_state_subdir_exists('.')

        mochitest = self._spawn(MochitestRunner)
        return mochitest.run_mochitest_test(test_file=test_file, suite=flavor,
            debugger=debugger)
