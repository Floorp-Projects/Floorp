# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import unittest

import mozprofile
import mozrunner


@unittest.skipIf(not os.environ.get('BROWSER_PATH'),
                 'No binary has been specified.')
class MozrunnerTestCase(unittest.TestCase):

    def setUp(self):
        self.pids = []
        self.threads = [ ]

        self.profile = mozprofile.FirefoxProfile()
        self.runner = mozrunner.FirefoxRunner(os.environ['BROWSER_PATH'],
                                              profile=self.profile)

    def tearDown(self):
        for thread in self.threads:
            thread.join()

        self.runner.cleanup()

        # Clean-up any left over and running processes
        for pid in self.pids:
            # TODO: Bug 925408
            # mozprocess is not able yet to kill specific processes
            pass
