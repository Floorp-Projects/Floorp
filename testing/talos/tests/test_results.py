#!/usr/bin/env python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
test talos results parsing

http://hg.mozilla.org/build/talos/file/tip/talos/results.py
"""

import unittest
import talos.filter
import talos.results

# example page load test results string
results_string = """_x_x_mozilla_page_load
_x_x_mozilla_page_load_details
|i|pagename|runs|
|0;gearflowers.svg;74;65;68;66;62
|1;composite-scale.svg;43;44;35;41;41
|2;composite-scale-opacity.svg;19;16;19;19;21
|3;composite-scale-rotate.svg;18;19;20;20;19
|4;composite-scale-rotate-opacity.svg;18;17;17;17;19
|5;hixie-001.xml;71836;15057;15063;57436;15061
|6;hixie-002.xml;53940;15057;38246;55323;31818
|7;hixie-003.xml;5027;5026;13540;31503;5031
|8;hixie-004.xml;5050;5054;5053;5054;5055
|9;hixie-005.xml;4568;4569;4562;4545;4567
|10;hixie-006.xml;5090;5165;5054;5015;5077
|11;hixie-007.xml;1628;1623;1623;1617;1622
"""


class TestPageloaderResults(unittest.TestCase):

    def test_parsing(self):
        """test our ability to parse results data"""
        results = talos.results.PageloaderResults(results_string)

        # ensure you got all of them
        self.assertEqual(len(results.results), 12)

        # test the indices
        indices = [i['index'] for i in results.results]
        self.assertEqual(indices, range(12))

        # test some pages
        pages = [i['page'] for i in results.results]
        comparison = ['hixie-00%d.xml' % i for i in range(1, 8)]
        self.assertEqual(pages[-len(comparison):], comparison)

        # test a few values
        last = [1628., 1623., 1623, 1617., 1622.]
        self.assertEqual(results.results[-1]['runs'], last)
        first = [74., 65., 68., 66., 62.]
        self.assertEqual(results.results[0]['runs'], first)

    def test_filter(self):
        """test PageloaderResults.filter function"""

        # parse the data
        results = talos.results.PageloaderResults(results_string)

        # apply some filters
        filters = [[talos.filter.ignore_first, [2]], [talos.filter.median]]
        filtered = results.filter(*filters)
        self.assertEqual(filtered[0][0], 66.)
        self.assertEqual(filtered[-1][0], 1622.)

        # apply some different filters
        filters = [[talos.filter.ignore_max, []], [max, []]]
        filtered = results.filter(*filters)
        self.assertEqual(filtered[0][0], 68.)
        self.assertEqual(filtered[-1][0], 1623.)

if __name__ == '__main__':
    unittest.main()
