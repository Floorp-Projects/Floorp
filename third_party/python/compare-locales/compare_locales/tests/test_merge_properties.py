# coding=utf8

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from codecs import decode, encode
import unittest

from compare_locales.merge import merge_channels


class TestMergeProperties(unittest.TestCase):
    name = "foo.properties"

    def test_no_changes(self):
        channels = (b"""
foo = Foo 1
""", b"""
foo = Foo 2
""")
        self.assertEqual(
            merge_channels(self.name, *channels), b"""
foo = Foo 1
""")

    def test_encoding(self):
        channels = (encode(u"""
foo = Foo 1…
""", "utf8"), encode(u"""
foo = Foo 2…
""", "utf8"))
        output = merge_channels(self.name, *channels)
        self.assertEqual(output, encode(u"""
foo = Foo 1…
""", "utf8"))

        u_output = decode(output, "utf8")
        self.assertEqual(u_output, u"""
foo = Foo 1…
""")
