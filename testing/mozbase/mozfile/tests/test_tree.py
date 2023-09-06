#!/usr/bin/env python
# coding=UTF-8

import os
import shutil
import tempfile
import unittest

import mozunit
from mozfile import tree


class TestTree(unittest.TestCase):
    """Test the tree function."""

    def test_unicode_paths(self):
        """Test creating tree structure from a Unicode path."""
        try:
            tmpdir = tempfile.mkdtemp(suffix="tmp🍪")
            os.mkdir(os.path.join(tmpdir, "dir🍪"))
            with open(os.path.join(tmpdir, "file🍪"), "w") as f:
                f.write("foo")

            self.assertEqual("{}\n├file🍪\n└dir🍪".format(tmpdir), tree(tmpdir))
        finally:
            shutil.rmtree(tmpdir)


if __name__ == "__main__":
    mozunit.main()
