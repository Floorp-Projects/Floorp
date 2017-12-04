# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import unittest
from compare_locales.merge import merge_channels


class TestMergeComments(unittest.TestCase):
    name = "foo.properties"

    def test_comment_added_in_first(self):
        channels = (b"""
foo = Foo 1
# Bar Comment 1
bar = Bar 1
""", b"""
foo = Foo 2
bar = Bar 2
""")
        self.assertMultiLineEqual(
            merge_channels(self.name, *channels), b"""
foo = Foo 1
# Bar Comment 1
bar = Bar 1
""")

    def test_comment_still_in_last(self):
        channels = (b"""
foo = Foo 1
bar = Bar 1
""", b"""
foo = Foo 2
# Bar Comment 2
bar = Bar 2
""")
        self.assertMultiLineEqual(
            merge_channels(self.name, *channels), b"""
foo = Foo 1
# Bar Comment 2
bar = Bar 1
""")

    def test_comment_changed(self):
        channels = (b"""
foo = Foo 1
# Bar Comment 1
bar = Bar 1
""", b"""
foo = Foo 2
# Bar Comment 2
bar = Bar 2
""")
        self.assertMultiLineEqual(
            merge_channels(self.name, *channels), b"""
foo = Foo 1
# Bar Comment 1
bar = Bar 1
""")


class TestMergeStandaloneComments(unittest.TestCase):
    name = "foo.properties"

    def test_comment_added_in_first(self):
        channels = (b"""
# Standalone Comment 1

# Foo Comment 1
foo = Foo 1
""", b"""
# Foo Comment 2
foo = Foo 2
""")
        self.assertMultiLineEqual(
            merge_channels(self.name, *channels), b"""
# Standalone Comment 1

# Foo Comment 1
foo = Foo 1
""")

    def test_comment_still_in_last(self):
        channels = (b"""
# Foo Comment 1
foo = Foo 1
""", b"""
# Standalone Comment 2

# Foo Comment 2
foo = Foo 2
""")
        self.assertMultiLineEqual(
            merge_channels(self.name, *channels), b"""
# Standalone Comment 2

# Foo Comment 1
foo = Foo 1
""")

    def test_comments_in_both(self):
        channels = (b"""
# Standalone Comment 1

# Foo Comment 1
foo = Foo 1
""", b"""
# Standalone Comment 2

# Foo Comment 2
foo = Foo 2
""")
        self.assertMultiLineEqual(
            merge_channels(self.name, *channels), b"""
# Standalone Comment 2

# Standalone Comment 1

# Foo Comment 1
foo = Foo 1
""")

    def test_identical_comments_in_both(self):
        channels = (b"""
# Standalone Comment

# Foo Comment 1
foo = Foo 1
""", b"""
# Standalone Comment

# Foo Comment 2
foo = Foo 2
""")
        self.assertMultiLineEqual(
            merge_channels(self.name, *channels), b"""
# Standalone Comment

# Foo Comment 1
foo = Foo 1
""")

    def test_standalone_which_is_attached_in_first(self):
        channels = (b"""
# Ambiguous Comment
foo = Foo 1

# Bar Comment 1
bar = Bar 1
""", b"""
# Ambiguous Comment

# Bar Comment 2
bar = Bar 2
""")
        self.assertMultiLineEqual(
            merge_channels(self.name, *channels), b"""
# Ambiguous Comment

foo = Foo 1

# Bar Comment 1
bar = Bar 1
""")

    def test_standalone_which_is_attached_in_second(self):
        channels = (b"""
# Ambiguous Comment

# Bar Comment 1
bar = Bar 1
""", b"""
# Ambiguous Comment
foo = Foo 1

# Bar Comment 2
bar = Bar 2
""")
        self.assertMultiLineEqual(
            merge_channels(self.name, *channels), b"""
# Ambiguous Comment
foo = Foo 1

# Bar Comment 1
bar = Bar 1
""")
