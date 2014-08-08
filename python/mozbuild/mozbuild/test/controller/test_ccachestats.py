# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import unicode_literals

import unittest

from mozunit import main

from mozbuild.controller.building import CCacheStats


class TestCcacheStats(unittest.TestCase):
    STAT_GARBAGE = """A garbage line which should be failed to parse"""

    STAT0 = """
    cache directory                     /home/tlin/.ccache
    cache hit (direct)                     0
    cache hit (preprocessed)               0
    cache miss                             0
    files in cache                         0
    cache size                             0 Kbytes
    max cache size                      16.0 Gbytes"""

    STAT1 = """
    cache directory                     /home/tlin/.ccache
    cache hit (direct)                   100
    cache hit (preprocessed)             200
    cache miss                          2500
    called for link                      180
    called for preprocessing               6
    compile failed                        11
    preprocessor error                     3
    bad compiler arguments                 6
    unsupported source language            9
    autoconf compile/link                 60
    unsupported compiler option            2
    no input file                         21
    files in cache                      7344
    cache size                           1.9 Gbytes
    max cache size                      16.0 Gbytes"""

    STAT2 = """
    cache directory                     /home/tlin/.ccache
    cache hit (direct)                  1900
    cache hit (preprocessed)             300
    cache miss                          2600
    called for link                      361
    called for preprocessing              12
    compile failed                        22
    preprocessor error                     6
    bad compiler arguments                12
    unsupported source language           18
    autoconf compile/link                120
    unsupported compiler option            4
    no input file                         48
    files in cache                      7392
    cache size                           2.0 Gbytes
    max cache size                      16.0 Gbytes"""

    def test_parse_garbage_stats_message(self):
        self.assertRaises(ValueError, CCacheStats, self.STAT_GARBAGE)

    def test_parse_zero_stats_message(self):
        stats = CCacheStats(self.STAT0)
        self.assertEqual(stats.cache_dir, "/home/tlin/.ccache")
        self.assertEqual(stats.hit_rates(), (0, 0, 0))

    def test_hit_rate_of_diff_stats(self):
        stats1 = CCacheStats(self.STAT1)
        stats2 = CCacheStats(self.STAT2)
        stats_diff = stats2 - stats1
        self.assertEqual(stats_diff.hit_rates(), (0.9, 0.05, 0.05))

    def test_stats_contains_data(self):
        stats0 = CCacheStats(self.STAT0)
        stats1 = CCacheStats(self.STAT1)
        stats2 = CCacheStats(self.STAT2)
        stats_diff_zero = stats1 - stats1
        stats_diff_negative1 = stats0 - stats1
        stats_diff_negative2 = stats1 - stats2

        self.assertFalse(stats0)
        self.assertTrue(stats1)
        self.assertTrue(stats2)
        self.assertFalse(stats_diff_zero)
        self.assertFalse(stats_diff_negative1)
        self.assertFalse(stats_diff_negative2)


if __name__ == '__main__':
    main()
