# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

from mozpack.path import (
    relpath,
    join,
    normpath,
    dirname,
    commonprefix,
    basename,
    split,
    splitext,
    basedir,
    match,
    rebase,
)
import unittest
import mozunit
import os


class TestPath(unittest.TestCase):
    SEP = os.sep

    def test_relpath(self):
        self.assertEqual(relpath("foo", "foo"), "")
        self.assertEqual(relpath(self.SEP.join(("foo", "bar")), "foo/bar"), "")
        self.assertEqual(relpath(self.SEP.join(("foo", "bar")), "foo"), "bar")
        self.assertEqual(
            relpath(self.SEP.join(("foo", "bar", "baz")), "foo"), "bar/baz"
        )
        self.assertEqual(relpath(self.SEP.join(("foo", "bar")), "foo/bar/baz"), "..")
        self.assertEqual(relpath(self.SEP.join(("foo", "bar")), "foo/baz"), "../bar")
        self.assertEqual(relpath("foo/", "foo"), "")
        self.assertEqual(relpath("foo/bar/", "foo"), "bar")

    def test_join(self):
        self.assertEqual(join("foo", "bar", "baz"), "foo/bar/baz")
        self.assertEqual(join("foo", "", "bar"), "foo/bar")
        self.assertEqual(join("", "foo", "bar"), "foo/bar")
        self.assertEqual(join("", "foo", "/bar"), "/bar")

    def test_normpath(self):
        self.assertEqual(
            normpath(self.SEP.join(("foo", "bar", "baz", "..", "qux"))), "foo/bar/qux"
        )

    def test_dirname(self):
        self.assertEqual(dirname("foo/bar/baz"), "foo/bar")
        self.assertEqual(dirname("foo/bar"), "foo")
        self.assertEqual(dirname("foo"), "")
        self.assertEqual(dirname("foo/bar/"), "foo/bar")

    def test_commonprefix(self):
        self.assertEqual(
            commonprefix(
                [self.SEP.join(("foo", "bar", "baz")), "foo/qux", "foo/baz/qux"]
            ),
            "foo/",
        )
        self.assertEqual(
            commonprefix([self.SEP.join(("foo", "bar", "baz")), "foo/qux", "baz/qux"]),
            "",
        )

    def test_basename(self):
        self.assertEqual(basename("foo/bar/baz"), "baz")
        self.assertEqual(basename("foo/bar"), "bar")
        self.assertEqual(basename("foo"), "foo")
        self.assertEqual(basename("foo/bar/"), "")

    def test_split(self):
        self.assertEqual(
            split(self.SEP.join(("foo", "bar", "baz"))), ["foo", "bar", "baz"]
        )

    def test_splitext(self):
        self.assertEqual(
            splitext(self.SEP.join(("foo", "bar", "baz.qux"))), ("foo/bar/baz", ".qux")
        )

    def test_basedir(self):
        foobarbaz = self.SEP.join(("foo", "bar", "baz"))
        self.assertEqual(basedir(foobarbaz, ["foo", "bar", "baz"]), "foo")
        self.assertEqual(basedir(foobarbaz, ["foo", "foo/bar", "baz"]), "foo/bar")
        self.assertEqual(basedir(foobarbaz, ["foo/bar", "foo", "baz"]), "foo/bar")
        self.assertEqual(basedir(foobarbaz, ["foo", "bar", ""]), "foo")
        self.assertEqual(basedir(foobarbaz, ["bar", "baz", ""]), "")

    def test_match(self):
        self.assertTrue(match("foo", ""))
        self.assertTrue(match("foo/bar/baz.qux", "foo/bar"))
        self.assertTrue(match("foo/bar/baz.qux", "foo"))
        self.assertTrue(match("foo", "*"))
        self.assertTrue(match("foo/bar/baz.qux", "foo/bar/*"))
        self.assertTrue(match("foo/bar/baz.qux", "foo/bar/*"))
        self.assertTrue(match("foo/bar/baz.qux", "foo/bar/*"))
        self.assertTrue(match("foo/bar/baz.qux", "foo/bar/*"))
        self.assertTrue(match("foo/bar/baz.qux", "foo/*/baz.qux"))
        self.assertTrue(match("foo/bar/baz.qux", "*/bar/baz.qux"))
        self.assertTrue(match("foo/bar/baz.qux", "*/*/baz.qux"))
        self.assertTrue(match("foo/bar/baz.qux", "*/*/*"))
        self.assertTrue(match("foo/bar/baz.qux", "foo/*/*"))
        self.assertTrue(match("foo/bar/baz.qux", "foo/*/*.qux"))
        self.assertTrue(match("foo/bar/baz.qux", "foo/b*/*z.qux"))
        self.assertTrue(match("foo/bar/baz.qux", "foo/b*r/ba*z.qux"))
        self.assertFalse(match("foo/bar/baz.qux", "foo/b*z/ba*r.qux"))
        self.assertTrue(match("foo/bar/baz.qux", "**"))
        self.assertTrue(match("foo/bar/baz.qux", "**/baz.qux"))
        self.assertTrue(match("foo/bar/baz.qux", "**/bar/baz.qux"))
        self.assertTrue(match("foo/bar/baz.qux", "foo/**/baz.qux"))
        self.assertTrue(match("foo/bar/baz.qux", "foo/**/*.qux"))
        self.assertTrue(match("foo/bar/baz.qux", "**/foo/bar/baz.qux"))
        self.assertTrue(match("foo/bar/baz.qux", "foo/**/bar/baz.qux"))
        self.assertTrue(match("foo/bar/baz.qux", "foo/**/bar/*.qux"))
        self.assertTrue(match("foo/bar/baz.qux", "foo/**/*.qux"))
        self.assertTrue(match("foo/bar/baz.qux", "**/*.qux"))
        self.assertFalse(match("foo/bar/baz.qux", "**.qux"))
        self.assertFalse(match("foo/bar", "foo/*/bar"))
        self.assertTrue(match("foo/bar/baz.qux", "foo/**/bar/**"))
        self.assertFalse(match("foo/nobar/baz.qux", "foo/**/bar/**"))
        self.assertTrue(match("foo/bar", "foo/**/bar/**"))

    def test_rebase(self):
        self.assertEqual(rebase("foo", "foo/bar", "bar/baz"), "baz")
        self.assertEqual(rebase("foo", "foo", "bar/baz"), "bar/baz")
        self.assertEqual(rebase("foo/bar", "foo", "baz"), "bar/baz")


if os.altsep:

    class TestAltPath(TestPath):
        SEP = os.altsep

    class TestReverseAltPath(TestPath):
        def setUp(self):
            sep = os.sep
            os.sep = os.altsep
            os.altsep = sep

        def tearDown(self):
            self.setUp()

    class TestAltReverseAltPath(TestReverseAltPath):
        SEP = os.altsep


if __name__ == "__main__":
    mozunit.main()
