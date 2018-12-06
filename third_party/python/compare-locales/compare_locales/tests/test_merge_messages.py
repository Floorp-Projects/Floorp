# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import unittest
from compare_locales.merge import merge_channels


class TestMergeTwo(unittest.TestCase):
    name = "foo.properties"

    def test_no_changes(self):
        channels = (b"""
foo = Foo 1
""", b"""
foo = Foo 2
""")
        self.assertEqual(
            merge_channels(self.name, channels), b"""
foo = Foo 1
""")

    def test_message_added_in_first(self):
        channels = (b"""
foo = Foo 1
bar = Bar 1
""", b"""
foo = Foo 2
""")
        self.assertEqual(
            merge_channels(self.name, channels), b"""
foo = Foo 1
bar = Bar 1
""")

    def test_message_still_in_last(self):
        channels = (b"""
foo = Foo 1
""", b"""
foo = Foo 2
bar = Bar 2
""")
        self.assertEqual(
            merge_channels(self.name, channels), b"""
foo = Foo 1
bar = Bar 2
""")

    def test_message_reordered(self):
        channels = (b"""
foo = Foo 1
bar = Bar 1
""", b"""
bar = Bar 2
foo = Foo 2
""")
        self.assertEqual(
            merge_channels(self.name, channels), b"""
foo = Foo 1
bar = Bar 1
""")


class TestMergeThree(unittest.TestCase):
    name = "foo.properties"

    def test_no_changes(self):
        channels = (b"""
foo = Foo 1
""", b"""
foo = Foo 2
""", b"""
foo = Foo 3
""")
        self.assertEqual(
            merge_channels(self.name, channels), b"""
foo = Foo 1
""")

    def test_message_still_in_last(self):
        channels = (b"""
foo = Foo 1
""", b"""
foo = Foo 2
""", b"""
foo = Foo 3
bar = Bar 3
""")
        self.assertEqual(
            merge_channels(self.name, channels), b"""
foo = Foo 1
bar = Bar 3
""")
