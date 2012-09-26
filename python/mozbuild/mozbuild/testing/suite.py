# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import unicode_literals

from mozbuild.base import MozbuildObject
from mozbuild.testing.xpcshell import XPCShellRunner
from mozbuild.testing.mochitest import MochitestRunner


class Suite(MozbuildObject):
    def run_suite(self, suite):
        """Run a named test suite.

        Recognized names are:

          all - All test suites
          mochitest-plain - Plain mochitests
          mochitest-chrome - mochitests with chrome
          mochitest-browser - mochitests with browser chrome
          xpcshell - xpcshell tests

        TODO support for other test suite types.
        """

        xpcshell = self._spawn(XPCShellRunner)
        mochitest = self._spawn(MochitestRunner)

        if suite == 'all':
            xpcshell.run_suite()
            mochitest.run_plain_suite()
            mochitest.run_chrome_suite()
            mochitest.run_browser_chrome_suite()
            return

        m = {
            'xpcshell': xpcshell.run_suite,
            'mochitest-plain': mochitest.run_plain_suite,
            'mochitest-chrome': mochitest.run_chrome_suite,
            'mochitest-browser': mochitest.run_browser_chrome_suite,
        }

        method = m.get(suite, None)

        if method is None:
            raise Exception('Unknown test suite: %s' % suite)

        return method()
