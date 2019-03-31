# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import unittest
from compare_locales.merge import merge_channels


class TestMergeFluent(unittest.TestCase):
    name = "foo.ftl"

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

    def test_attribute_in_first(self):
        channels = (b"""
foo = Foo 1
    .attr = Attr 1
""", b"""
foo = Foo 2
""")
        self.assertEqual(
            merge_channels(self.name, channels), b"""
foo = Foo 1
    .attr = Attr 1
""")

    def test_attribute_in_last(self):
        channels = (b"""
foo = Foo 1
""", b"""
foo = Foo 2
    .attr = Attr 2
""")
        self.assertEqual(
            merge_channels(self.name, channels), b"""
foo = Foo 1
""")

    def test_junk_in_first(self):
        channels = (b"""\
line of junk
""", b"""\
one = entry
""")
        self.assertMultiLineEqual(
            merge_channels(self.name, channels).decode('utf-8'),
            """\
one = entry
line of junk
"""
        )

    def test_junk_in_last(self):
        channels = (b"""\
one = entry
""", b"""\
line of junk
""")
        self.assertMultiLineEqual(
            merge_channels(self.name, channels).decode('utf-8'),
            """\
line of junk
one = entry
"""
        )

    def test_attribute_changed(self):
        channels = (b"""
foo = Foo 1
    .attr = Attr 1
""", b"""
foo = Foo 2
    .attr = Attr 2
""")
        self.assertEqual(
            merge_channels(self.name, channels), b"""
foo = Foo 1
    .attr = Attr 1
""")

    def test_group_comment_in_first(self):
        channels = (b"""
## Group Comment 1
foo = Foo 1
""", b"""
foo = Foo 2
""")
        self.assertEqual(
            merge_channels(self.name, channels), b"""
## Group Comment 1
foo = Foo 1
""")

    def test_group_comment_in_last(self):
        channels = (b"""
foo = Foo 1
""", b"""
## Group Comment 2
foo = Foo 2
""")
        self.assertEqual(
            merge_channels(self.name, channels), b"""
## Group Comment 2
foo = Foo 1
""")

    def test_group_comment_changed(self):
        channels = (b"""
## Group Comment 1
foo = Foo 1
""", b"""
## Group Comment 2
foo = Foo 2
""")
        self.assertEqual(
            merge_channels(self.name, channels), b"""
## Group Comment 2
## Group Comment 1
foo = Foo 1
""")

    def test_group_comment_and_section(self):
        channels = (b"""
## Group Comment
foo = Foo 1
""", b"""
// Section Comment
[[ Section ]]
foo = Foo 2
""")
        self.assertEqual(
            merge_channels(self.name, channels), b"""
// Section Comment
[[ Section ]]
## Group Comment
foo = Foo 1
""")

    def test_message_comment_in_first(self):
        channels = (b"""
# Comment 1
foo = Foo 1
""", b"""
foo = Foo 2
""")
        self.assertEqual(
            merge_channels(self.name, channels), b"""
# Comment 1
foo = Foo 1
""")

    def test_message_comment_in_last(self):
        channels = (b"""
foo = Foo 1
""", b"""
# Comment 2
foo = Foo 2
""")
        self.assertEqual(
            merge_channels(self.name, channels), b"""
foo = Foo 1
""")

    def test_message_comment_changed(self):
        channels = (b"""
# Comment 1
foo = Foo 1
""", b"""
# Comment 2
foo = Foo 2
""")
        self.assertEqual(
            merge_channels(self.name, channels), b"""
# Comment 1
foo = Foo 1
""")

    def test_standalone_comment_in_first(self):
        channels = (b"""
foo = Foo 1

# Comment 1
""", b"""
foo = Foo 2
""")
        self.assertEqual(
            merge_channels(self.name, channels), b"""
foo = Foo 1

# Comment 1
""")

    def test_standalone_comment_in_last(self):
        channels = (b"""
foo = Foo 1
""", b"""
foo = Foo 2

# Comment 2
""")
        self.assertEqual(
            merge_channels(self.name, channels), b"""
foo = Foo 1

# Comment 2
""")

    def test_standalone_comment_changed(self):
        channels = (b"""
foo = Foo 1

# Comment 1
""", b"""
foo = Foo 2

# Comment 2
""")
        self.assertEqual(
            merge_channels(self.name, channels), b"""
foo = Foo 1

# Comment 2

# Comment 1
""")

    def test_resource_comment_in_first(self):
        channels = (b"""
### Resource Comment 1

foo = Foo 1
""", b"""
foo = Foo 2
""")
        self.assertEqual(
            merge_channels(self.name, channels), b"""
### Resource Comment 1

foo = Foo 1
""")

    def test_resource_comment_in_last(self):
        channels = (b"""
foo = Foo 1
""", b"""
### Resource Comment 1

foo = Foo 2
""")
        self.assertEqual(
            merge_channels(self.name, channels), b"""
### Resource Comment 1

foo = Foo 1
""")

    def test_resource_comment_changed(self):
        channels = (b"""
### Resource Comment 1

foo = Foo 1
""", b"""
### Resource Comment 2

foo = Foo 2
""")
        self.assertEqual(
            merge_channels(self.name, channels), b"""
### Resource Comment 2

### Resource Comment 1

foo = Foo 1
""")
