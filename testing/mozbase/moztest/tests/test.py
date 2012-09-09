# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import math
import time
import unittest

from moztest.results import TestContext, TestResult, TestResultCollection


class Result(unittest.TestCase):

    def test_results(self):
        self.assertRaises(AssertionError,
                          lambda: TestResult('test', result_expected='hello'))
        t = TestResult('test')
        self.assertRaises(ValueError, lambda: t.finish(result='good bye'))

    def test_time(self):
        now = time.time()
        t = TestResult('test')
        time.sleep(1)
        t.finish('PASS')
        duration = time.time() - now
        self.assertTrue(math.fabs(duration - t.duration) < 1)

    def test_custom_time(self):
        t = TestResult('test', time_start=0)
        t.finish(result='PASS', time_end=1000)
        self.assertEqual(t.duration, 1000)


class Collection(unittest.TestCase):

    def setUp(self):
        c1 = TestContext('host1')
        c2 = TestContext('host2')
        c3 = TestContext('host2')
        c3.os = 'B2G'
        c4 = TestContext('host1')

        t1 = TestResult('t1', context=c1)
        t2 = TestResult('t2', context=c2)
        t3 = TestResult('t3', context=c3)
        t4 = TestResult('t4', context=c4)

        self.collection = TestResultCollection('tests')
        self.collection.extend([t1, t2, t3, t4])

    def test_unique_contexts(self):
        self.assertEqual(len(self.collection.contexts), 3)

if __name__ == '__main__':
    unittest.main()
