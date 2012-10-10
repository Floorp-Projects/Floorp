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


