# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, unicode_literals

import datetime
import unittest

from mozunit import main

from taskgraph.cron.util import (
    match_utc,
)


class TestMatchUtc(unittest.TestCase):

    def test_hour_minute(self):
        params = {'time': datetime.datetime(2017, 01, 26, 16, 30, 0)}
        self.assertFalse(match_utc(params, hour=4, minute=30))
        self.assertTrue(match_utc(params, hour=16, minute=30))
        self.assertFalse(match_utc(params, hour=16, minute=0))

    def test_hour_only(self):
        params = {'time': datetime.datetime(2017, 01, 26, 16, 0, 0)}
        self.assertFalse(match_utc(params, hour=0))
        self.assertFalse(match_utc(params, hour=4))
        self.assertTrue(match_utc(params, hour=16))
        params = {'time': datetime.datetime(2017, 01, 26, 16, 15, 0)}
        self.assertFalse(match_utc(params, hour=0))
        self.assertFalse(match_utc(params, hour=4))
        self.assertTrue(match_utc(params, hour=16))
        params = {'time': datetime.datetime(2017, 01, 26, 16, 30, 0)}
        self.assertFalse(match_utc(params, hour=0))
        self.assertFalse(match_utc(params, hour=4))
        self.assertTrue(match_utc(params, hour=16))
        params = {'time': datetime.datetime(2017, 01, 26, 16, 45, 0)}
        self.assertFalse(match_utc(params, hour=0))
        self.assertFalse(match_utc(params, hour=4))
        self.assertTrue(match_utc(params, hour=16))

    def test_minute_only(self):
        params = {'time': datetime.datetime(2017, 01, 26, 13, 0, 0)}
        self.assertTrue(match_utc(params, minute=0))
        self.assertFalse(match_utc(params, minute=15))
        self.assertFalse(match_utc(params, minute=30))
        self.assertFalse(match_utc(params, minute=45))

    def test_zeroes(self):
        params = {'time': datetime.datetime(2017, 01, 26, 0, 0, 0)}
        self.assertTrue(match_utc(params, minute=0))
        self.assertTrue(match_utc(params, hour=0))
        self.assertFalse(match_utc(params, hour=1))
        self.assertFalse(match_utc(params, minute=15))
        self.assertFalse(match_utc(params, minute=30))
        self.assertFalse(match_utc(params, minute=45))

    def test_invalid_minute(self):
        params = {'time': datetime.datetime(2017, 01, 26, 13, 0, 0)}
        self.assertRaises(Exception, lambda:
                          match_utc(params, minute=1))

if __name__ == '__main__':
    main()
