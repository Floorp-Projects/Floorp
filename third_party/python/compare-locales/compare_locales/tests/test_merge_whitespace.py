# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import unittest
from compare_locales.merge import merge_channels


class TestMergeWhitespace(unittest.TestCase):
    name = "foo.properties"

    def test_trailing_spaces(self):
        channels = (b"""
foo = Foo 1
      """, b"""
foo = Foo 2
""")
        self.assertEqual(
            merge_channels(self.name, *channels), b"""
foo = Foo 1
      """)

    def test_blank_lines_between_messages(self):
        channels = (b"""
foo = Foo 1

bar = Bar 1
""", b"""
foo = Foo 2
bar = Bar 2
""")
        self.assertEqual(
            merge_channels(self.name, *channels), b"""
foo = Foo 1

bar = Bar 1
""")

    def test_no_eol(self):
        channels = (b"""
foo = Foo 1""", b"""
foo = Foo 2
bar = Bar 2
""")
        self.assertEqual(
            merge_channels(self.name, *channels), b"""
foo = Foo 1
bar = Bar 2
""")

    def test_still_in_last_with_blank(self):
        channels = (b"""

foo = Foo 1

baz = Baz 1

""", b"""

foo = Foo 2

bar = Bar 2

baz = Baz 2

""")
        self.assertEqual(
            merge_channels(self.name, *channels), b"""

foo = Foo 1

bar = Bar 2

baz = Baz 1

""")
