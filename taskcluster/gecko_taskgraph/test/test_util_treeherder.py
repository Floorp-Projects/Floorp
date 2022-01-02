# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


import unittest
from gecko_taskgraph.util.treeherder import split_symbol, join_symbol, add_suffix
from mozunit import main


class TestSymbols(unittest.TestCase):
    def test_split_no_group(self):
        self.assertEqual(split_symbol("xy"), ("?", "xy"))

    def test_split_with_group(self):
        self.assertEqual(split_symbol("ab(xy)"), ("ab", "xy"))

    def test_join_no_group(self):
        self.assertEqual(join_symbol("?", "xy"), "xy")

    def test_join_with_group(self):
        self.assertEqual(join_symbol("ab", "xy"), "ab(xy)")

    def test_add_suffix_no_group(self):
        self.assertEqual(add_suffix("xy", 1), "xy1")

    def test_add_suffix_with_group(self):
        self.assertEqual(add_suffix("ab(xy)", 1), "ab(xy1)")


if __name__ == "__main__":
    main()
