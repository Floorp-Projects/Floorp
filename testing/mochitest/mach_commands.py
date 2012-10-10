# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import unicode_literals

import os

from mozbuild.base import MozbuildObject
from mozbuild.testing.test import TestRunner

from mach.base import (
    CommandArgument,
    CommandProvider,
    Command,
)


generic_help = 'Test to run. Can be specified as a single file, a ' +\
'directory, or omitted. If omitted, the entire test suite is executed.'


class MochitestRunner(TestRunner):
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

    def run_all(self):
        self.run_plain_suite()
        self.run_chrome_suite()
        self.run_browser_chrome_suite()

    def run_mochitest_test(self, test_file=None, suite=None):
        """Runs a mochitest.

        test_file is a path to a test file. It can be a relative path from the
        top source directory, an absolute filename, or a directory containing
        test files.

        suite is the type of mochitest to run. It can be one of ('plain',
        'chrome', 'browser').
        """

        # TODO hook up harness via native Python
        target = None
        if suite == 'plain':
            target = 'mochitest-plain'
        elif suite == 'chrome':
            target = 'mochitest-chrome'
        elif suite == 'browser':
            target = 'mochitest-browser-chrome'
        elif suite == 'a11y':
            target = 'mochitest-a11y'
        else:
            raise Exception('None or unrecognized mochitest suite type.')

        if test_file:
            path = self._parse_test_path(test_file)['normalized']
            if not os.path.exists(path):
                raise Exception('No manifest file was found at %s.' % path)
            env = {'TEST_PATH': path}
        else:
            env = {}

        self._run_make(directory='.', target=target, append_env=env)


@CommandProvider
class MachCommands(MozbuildObject):
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
        mochitest = self._spawn(MochitestRunner)
        mochitest.run_mochitest_test(test_file, flavor)
