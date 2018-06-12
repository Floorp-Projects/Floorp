# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import unicode_literals

import time
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

    STAT3 = """
    cache directory                     /Users/tlin/.ccache
    primary config                      /Users/tlin/.ccache/ccache.conf
    secondary config      (readonly)    /usr/local/Cellar/ccache/3.2/etc/ccache.conf
    cache hit (direct)                 12004
    cache hit (preprocessed)            1786
    cache miss                         26348
    called for link                     2338
    called for preprocessing            6313
    compile failed                       399
    preprocessor error                   390
    bad compiler arguments                86
    unsupported source language           66
    autoconf compile/link               2439
    unsupported compiler option          187
    no input file                       1068
    files in cache                     18044
    cache size                           7.5 GB
    max cache size                       8.6 GB
    """

    STAT4 = """
    cache directory                     /Users/tlin/.ccache
    primary config                      /Users/tlin/.ccache/ccache.conf
    secondary config      (readonly)    /usr/local/Cellar/ccache/3.2.1/etc/ccache.conf
    cache hit (direct)                 21039
    cache hit (preprocessed)            2315
    cache miss                         39370
    called for link                     3651
    called for preprocessing            6693
    compile failed                       723
    ccache internal error                  1
    preprocessor error                   588
    bad compiler arguments               128
    unsupported source language           99
    autoconf compile/link               3669
    unsupported compiler option          187
    no input file                       1711
    files in cache                     18313
    cache size                           6.3 GB
    max cache size                       6.0 GB
    """

    STAT5 = """
    cache directory                     /Users/tlin/.ccache
    primary config                      /Users/tlin/.ccache/ccache.conf
    secondary config      (readonly)    /usr/local/Cellar/ccache/3.2.1/etc/ccache.conf
    cache hit (direct)                 21039
    cache hit (preprocessed)            2315
    cache miss                         39372
    called for link                     3653
    called for preprocessing            6693
    compile failed                       723
    ccache internal error                  1
    preprocessor error                   588
    bad compiler arguments               128
    unsupported source language           99
    autoconf compile/link               3669
    unsupported compiler option          187
    no input file                       1711
    files in cache                     17411
    cache size                           6.0 GB
    max cache size                       6.0 GB
    """

    STAT6 = """
    cache directory                     /Users/tlin/.ccache
    primary config                      /Users/tlin/.ccache/ccache.conf
    secondary config      (readonly)    /usr/local/Cellar/ccache/3.3.2/etc/ccache.conf
    cache hit (direct)                319287
    cache hit (preprocessed)          125987
    cache miss                        749959
    cache hit rate                     37.25 %
    called for link                    87978
    called for preprocessing          418591
    multiple source files               1861
    compiler produced no output          122
    compiler produced empty output       174
    compile failed                     14330
    ccache internal error                  1
    preprocessor error                  9459
    can't use precompiled header           4
    bad compiler arguments              2077
    unsupported source language        18195
    autoconf compile/link              51485
    unsupported compiler option          322
    no input file                     309538
    cleanups performed                     1
    files in cache                     17358
    cache size                          15.4 GB
    max cache size                      17.2 GB
    """

    STAT7 = """
    cache directory                     /Users/tlin/.ccache
    primary config                      /Users/tlin/.ccache/ccache.conf
    secondary config      (readonly)    /usr/local/Cellar/ccache/3.3.3/etc/ccache.conf
    cache hit (direct)                 27035
    cache hit (preprocessed)           13939
    cache miss                         62630
    cache hit rate                     39.55 %
    called for link                     1280
    called for preprocessing             736
    compile failed                       550
    preprocessor error                   638
    bad compiler arguments                20
    autoconf compile/link               1751
    unsupported code directive             2
    no input file                       2378
    cleanups performed                  1792
    files in cache                      3479
    cache size                           4.4 GB
    max cache size                       5.0 GB
    """

    # Substitute a locally-generated timestamp because the timestamp format is
    # locale-dependent.
    STAT8 = """
    cache directory                     /home/psimonyi/.ccache
    primary config                      /home/psimonyi/.ccache/ccache.conf
    secondary config      (readonly)    /etc/ccache.conf
    stats zero time                     {timestamp}
    cache hit (direct)                   571
    cache hit (preprocessed)            1203
    cache miss                         11747
    cache hit rate                     13.12 %
    called for link                      623
    called for preprocessing            7194
    compile failed                        32
    preprocessor error                   137
    bad compiler arguments                 4
    autoconf compile/link                348
    no input file                        162
    cleanups performed                    77
    files in cache                     13464
    cache size                           6.2 GB
    max cache size                       7.0 GB
    """.format(timestamp=time.strftime('%c'))

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

    def test_stats_version32(self):
        stat2 = CCacheStats(self.STAT2)
        stat3 = CCacheStats(self.STAT3)
        stats_diff = stat3 - stat2
        self.assertTrue(stat3)
        self.assertTrue(stats_diff)

    def test_cache_size_shrinking(self):
        stat4 = CCacheStats(self.STAT4)
        stat5 = CCacheStats(self.STAT5)
        stats_diff = stat5 - stat4
        self.assertTrue(stat4)
        self.assertTrue(stat5)
        self.assertTrue(stats_diff)

    def test_stats_version33(self):
        # Test stats for 3.3.2.
        stat3 = CCacheStats(self.STAT3)
        stat6 = CCacheStats(self.STAT6)
        stats_diff = stat6 - stat3
        self.assertTrue(stat6)
        self.assertTrue(stat3)
        self.assertTrue(stats_diff)

        # Test stats for 3.3.3.
        stat7 = CCacheStats(self.STAT7)
        self.assertTrue(stat7)

    def test_stats_version34(self):
        # Test parsing 3.4 output.
        stat8 = CCacheStats(self.STAT8)
        self.assertTrue(stat8)

if __name__ == '__main__':
    main()
