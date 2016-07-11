# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import unittest
from taskgraph.util.treeherder import split_symbol, join_symbol


class TestSymbols(unittest.TestCase):

    def test_split_no_group(self):
        self.assertEqual(split_symbol('xy'), ('?', 'xy'))

    def test_split_with_group(self):
        self.assertEqual(split_symbol('ab(xy)'), ('ab', 'xy'))

    def test_join_no_group(self):
        self.assertEqual(join_symbol('?', 'xy'), 'xy')

    def test_join_with_group(self):
        self.assertEqual(join_symbol('ab', 'xy'), 'ab(xy)')
