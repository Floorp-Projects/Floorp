#!/usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import mock
import mozunit

import mozrunnertest


class MozrunnerCrashTestCase(mozrunnertest.MozrunnerTestCase):

    @mock.patch('mozcrash.log_crashes', return_value=2)
    def test_crash_count_with_logger(self, log_crashes):
        self.assertEqual(self.runner.crashed, 0)
        self.assertEqual(self.runner.check_for_crashes(), 2)
        self.assertEqual(self.runner.crashed, 2)
        self.assertEqual(self.runner.check_for_crashes(), 2)
        self.assertEqual(self.runner.crashed, 4)

        log_crashes.return_value = 0
        self.assertEqual(self.runner.check_for_crashes(), 0)
        self.assertEqual(self.runner.crashed, 4)

    @mock.patch('mozcrash.check_for_crashes', return_value=2)
    def test_crash_count_without_logger(self, check_for_crashes):
        self.runner.logger = None

        self.assertEqual(self.runner.crashed, 0)
        self.assertEqual(self.runner.check_for_crashes(), 2)
        self.assertEqual(self.runner.crashed, 2)
        self.assertEqual(self.runner.check_for_crashes(), 2)
        self.assertEqual(self.runner.crashed, 4)

        check_for_crashes.return_value = 0
        self.assertEqual(self.runner.check_for_crashes(), 0)
        self.assertEqual(self.runner.crashed, 4)


if __name__ == '__main__':
    mozunit.main()
