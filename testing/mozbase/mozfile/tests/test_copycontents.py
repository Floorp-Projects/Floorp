#!/usr/bin/env python

from __future__ import absolute_import

import os
import shutil
import unittest

import mozunit

import mozfile

import stubs


class MozfileCopyContentsTestCase(unittest.TestCase):
    """Test our ability to copy the contents of directories"""

    def _directory_is_subset(self, set_, subset_):
        """
        Confirm that all the contents of 'subset_' are contained in 'set_'
        """
        names = os.listdir(subset_)
        for name in names:
            full_set_path = os.path.join(set_, name)
            full_subset_path = os.path.join(subset_, name)
            if os.path.isdir(full_subset_path):
                self.assertTrue(os.path.isdir(full_set_path))
                self._directory_is_subset(full_set_path, full_subset_path)
            elif os.path.islink(full_subset_path):
                self.assertTrue(os.path.islink(full_set_path))
            else:
                self.assertTrue(os.stat(full_set_path))

    def _directories_are_equal(self, dir1, dir2):
        """
        Confirm that the contents of 'dir1' are the same as 'dir2'
        """
        names1 = os.listdir(dir1)
        names2 = os.listdir(dir2)
        self.assertTrue(len(names1) == len(names2))
        for name in names1:
            self.assertTrue(name in names2)
            dir1_path = os.path.join(dir1, name)
            dir2_path = os.path.join(dir2, name)
            if os.path.isdir(dir1_path):
                self.assertTrue(os.path.isdir(dir2_path))
                self._directories_are_equal(dir1_path, dir2_path)
            elif os.path.islink(dir1_path):
                self.assertTrue(os.path.islink(dir2_path))
            else:
                self.assertTrue(os.stat(dir2_path))

    def test_copy_empty_directory(self):
        tempdir = stubs.create_empty_stub()
        dstdir = stubs.create_empty_stub()
        self.assertTrue(os.path.isdir(tempdir))

        mozfile.copy_contents(tempdir, dstdir)
        self._directories_are_equal(dstdir, tempdir)

        if os.path.isdir(tempdir):
            shutil.rmtree(tempdir)
        if os.path.isdir(dstdir):
            shutil.rmtree(dstdir)

    def test_copy_full_directory(self):
        tempdir = stubs.create_stub()
        dstdir = stubs.create_empty_stub()
        self.assertTrue(os.path.isdir(tempdir))

        mozfile.copy_contents(tempdir, dstdir)
        self._directories_are_equal(dstdir, tempdir)

        if os.path.isdir(tempdir):
            shutil.rmtree(tempdir)
        if os.path.isdir(dstdir):
            shutil.rmtree(dstdir)

    def test_copy_full_directory_with_existing_file(self):
        tempdir = stubs.create_stub()
        dstdir = stubs.create_empty_stub()

        filename = "i_dont_exist_in_tempdir"
        f = open(os.path.join(dstdir, filename), "w")
        f.write("Hello World")
        f.close()

        self.assertTrue(os.path.isdir(tempdir))

        mozfile.copy_contents(tempdir, dstdir)
        self._directory_is_subset(dstdir, tempdir)
        self.assertTrue(os.path.exists(os.path.join(dstdir, filename)))

        if os.path.isdir(tempdir):
            shutil.rmtree(tempdir)
        if os.path.isdir(dstdir):
            shutil.rmtree(dstdir)

    def test_copy_full_directory_with_overlapping_file(self):
        tempdir = stubs.create_stub()
        dstdir = stubs.create_empty_stub()

        filename = "i_do_exist_in_tempdir"
        for d in [tempdir, dstdir]:
            f = open(os.path.join(d, filename), "w")
            f.write("Hello " + d)
            f.close()

        self.assertTrue(os.path.isdir(tempdir))
        self.assertTrue(os.path.exists(os.path.join(tempdir, filename)))
        self.assertTrue(os.path.exists(os.path.join(dstdir, filename)))

        line = open(os.path.join(dstdir, filename), "r").readlines()[0]
        self.assertTrue(line == "Hello " + dstdir)

        mozfile.copy_contents(tempdir, dstdir)

        line = open(os.path.join(dstdir, filename), "r").readlines()[0]
        self.assertTrue(line == "Hello " + tempdir)
        self._directories_are_equal(tempdir, dstdir)

        if os.path.isdir(tempdir):
            shutil.rmtree(tempdir)
        if os.path.isdir(dstdir):
            shutil.rmtree(dstdir)


if __name__ == "__main__":
    mozunit.main()
