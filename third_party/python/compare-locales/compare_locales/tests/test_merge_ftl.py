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
            merge_channels(self.name, *channels), b"""
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
            merge_channels(self.name, *channels), b"""
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
            merge_channels(self.name, *channels), b"""
foo = Foo 1
""")

    def test_attribute_changed(self):
        channels = (b"""
foo = Foo 1
    .attr = Attr 1
""", b"""
foo = Foo 2
    .attr = Attr 2
""")
        self.assertEqual(
            merge_channels(self.name, *channels), b"""
foo = Foo 1
    .attr = Attr 1
""")

    def test_tag_in_first(self):
        channels = (b"""
foo = Foo 1
    #tag
""", b"""
foo = Foo 2
""")
        self.assertEqual(
            merge_channels(self.name, *channels), b"""
foo = Foo 1
    #tag
""")

    def test_tag_in_last(self):
        channels = (b"""
foo = Foo 1
""", b"""
foo = Foo 2
    #tag
""")
        self.assertEqual(
            merge_channels(self.name, *channels), b"""
foo = Foo 1
""")

    def test_tag_changed(self):
        channels = (b"""
foo = Foo 1
    #tag1
""", b"""
foo = Foo 2
    #tag2
""")
        self.assertEqual(
            merge_channels(self.name, *channels), b"""
foo = Foo 1
    #tag1
""")

    def test_section_in_first(self):
        channels = (b"""
[[ Section 1 ]]
foo = Foo 1
""", b"""
foo = Foo 2
""")
        self.assertEqual(
            merge_channels(self.name, *channels), b"""
[[ Section 1 ]]
foo = Foo 1
""")

    def test_section_in_last(self):
        channels = (b"""
foo = Foo 1
""", b"""
[[ Section 2 ]]
foo = Foo 2
""")
        self.assertEqual(
            merge_channels(self.name, *channels), b"""
[[ Section 2 ]]
foo = Foo 1
""")

    def test_section_changed(self):
        channels = (b"""
[[ Section 1 ]]
foo = Foo 1
""", b"""
[[ Section 2 ]]
foo = Foo 2
""")
        self.assertEqual(
            merge_channels(self.name, *channels), b"""
[[ Section 2 ]]
[[ Section 1 ]]
foo = Foo 1
""")

    def test_message_comment_in_first(self):
        channels = (b"""
// Comment 1
foo = Foo 1
""", b"""
foo = Foo 2
""")
        self.assertEqual(
            merge_channels(self.name, *channels), b"""
// Comment 1
foo = Foo 1
""")

    def test_message_comment_in_last(self):
        channels = (b"""
foo = Foo 1
""", b"""
// Comment 2
foo = Foo 2
""")
        self.assertEqual(
            merge_channels(self.name, *channels), b"""
foo = Foo 1
""")

    def test_message_comment_changed(self):
        channels = (b"""
// Comment 1
foo = Foo 1
""", b"""
// Comment 2
foo = Foo 2
""")
        self.assertEqual(
            merge_channels(self.name, *channels), b"""
// Comment 1
foo = Foo 1
""")

    def test_section_comment_in_first(self):
        channels = (b"""
// Comment 1
[[ Section ]]
""", b"""
[[ Section ]]
""")
        self.assertEqual(
            merge_channels(self.name, *channels), b"""
// Comment 1
[[ Section ]]
""")

    def test_section_comment_in_last(self):
        channels = (b"""
[[ Section ]]
""", b"""
// Comment 2
[[ Section ]]
""")
        self.assertEqual(
            merge_channels(self.name, *channels), b"""
[[ Section ]]
""")

    def test_section_comment_changed(self):
        channels = (b"""
// Comment 1
[[ Section ]]
""", b"""
// Comment 2
[[ Section ]]
""")
        self.assertEqual(
            merge_channels(self.name, *channels), b"""
// Comment 1
[[ Section ]]
""")

    def test_standalone_comment_in_first(self):
        channels = (b"""
foo = Foo 1

// Comment 1
""", b"""
foo = Foo 2
""")
        self.assertEqual(
            merge_channels(self.name, *channels), b"""
foo = Foo 1

// Comment 1
""")

    def test_standalone_comment_in_last(self):
        channels = (b"""
foo = Foo 1
""", b"""
foo = Foo 2

// Comment 2
""")
        self.assertEqual(
            merge_channels(self.name, *channels), b"""
foo = Foo 1

// Comment 2
""")

    def test_standalone_comment_changed(self):
        channels = (b"""
foo = Foo 1

// Comment 1
""", b"""
foo = Foo 2

// Comment 2
""")
        self.assertEqual(
            merge_channels(self.name, *channels), b"""
foo = Foo 1

// Comment 2

// Comment 1
""")

    def test_resource_comment_in_first(self):
        channels = (b"""
// Resource Comment 1

foo = Foo 1
""", b"""
foo = Foo 2
""")
        self.assertEqual(
            merge_channels(self.name, *channels), b"""
// Resource Comment 1

foo = Foo 1
""")

    def test_resource_comment_in_last(self):
        channels = (b"""
foo = Foo 1
""", b"""
// Resource Comment 1

foo = Foo 2
""")
        self.assertEqual(
            merge_channels(self.name, *channels), b"""
// Resource Comment 1

foo = Foo 1
""")

    def test_resource_comment_changed(self):
        channels = (b"""
// Resource Comment 1

foo = Foo 1
""", b"""
// Resource Comment 2

foo = Foo 2
""")
        self.assertEqual(
            merge_channels(self.name, *channels), b"""
// Resource Comment 2

// Resource Comment 1

foo = Foo 1
""")
