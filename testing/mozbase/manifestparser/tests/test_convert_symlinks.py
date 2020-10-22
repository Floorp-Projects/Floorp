#!/usr/bin/env python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

import os
import shutil
import tempfile
import unittest

import mozunit

from manifestparser import convert, ManifestParser


class TestSymlinkConversion(unittest.TestCase):
    """
    test conversion of a directory tree with symlinks to a manifest structure
    """

    def create_stub(self, directory=None):
        """stub out a directory with files in it"""

        files = ('foo', 'bar', 'fleem')
        if directory is None:
            directory = tempfile.mkdtemp()
        for i in files:
            open(os.path.join(directory, i), 'w').write(i)
        subdir = os.path.join(directory, 'subdir')
        os.mkdir(subdir)
        open(os.path.join(subdir, 'subfile'), 'w').write('baz')
        return directory

    def test_relpath(self):
        """test convert `relative_to` functionality"""

        oldcwd = os.getcwd()
        stub = self.create_stub()
        try:
            # subdir with in-memory manifest
            files = ['../bar', '../fleem', '../foo', 'subfile']
            subdir = os.path.join(stub, 'subdir')
            os.chdir(subdir)
            parser = convert([stub], relative_to='.')
            self.assertEqual([i['name'] for i in parser.tests],
                             files)
        except BaseException:
            raise
        finally:
            shutil.rmtree(stub)
            os.chdir(oldcwd)

    @unittest.skipIf(not hasattr(os, 'symlink'),
                     "symlinks unavailable on this platform")
    def test_relpath_symlink(self):
        """
        Ensure `relative_to` works in a symlink.
        Not available on windows.
        """

        oldcwd = os.getcwd()
        workspace = tempfile.mkdtemp()
        try:
            tmpdir = os.path.join(workspace, 'directory')
            os.makedirs(tmpdir)
            linkdir = os.path.join(workspace, 'link')
            os.symlink(tmpdir, linkdir)
            self.create_stub(tmpdir)

            # subdir with in-memory manifest
            files = ['../bar', '../fleem', '../foo', 'subfile']
            subdir = os.path.join(linkdir, 'subdir')
            os.chdir(os.path.realpath(subdir))
            for directory in (tmpdir, linkdir):
                parser = convert([directory], relative_to='.')
                self.assertEqual([i['name'] for i in parser.tests],
                                 files)
        finally:
            shutil.rmtree(workspace)
            os.chdir(oldcwd)

        # a more complicated example
        oldcwd = os.getcwd()
        workspace = tempfile.mkdtemp()
        try:
            tmpdir = os.path.join(workspace, 'directory')
            os.makedirs(tmpdir)
            linkdir = os.path.join(workspace, 'link')
            os.symlink(tmpdir, linkdir)
            self.create_stub(tmpdir)
            files = ['../bar', '../fleem', '../foo', 'subfile']
            subdir = os.path.join(linkdir, 'subdir')
            subsubdir = os.path.join(subdir, 'sub')
            os.makedirs(subsubdir)
            linksubdir = os.path.join(linkdir, 'linky')
            linksubsubdir = os.path.join(subsubdir, 'linky')
            os.symlink(subdir, linksubdir)
            os.symlink(subdir, linksubsubdir)
            for dest in (subdir,):
                os.chdir(dest)
                for directory in (tmpdir, linkdir):
                    parser = convert([directory], relative_to='.')
                    self.assertEqual([i['name'] for i in parser.tests],
                                     files)
        finally:
            shutil.rmtree(workspace)
            os.chdir(oldcwd)

    @unittest.skipIf(not hasattr(os, 'symlink'),
                     "symlinks unavailable on this platform")
    def test_recursion_symlinks(self):
        workspace = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, workspace)

        # create two dirs
        os.makedirs(os.path.join(workspace, 'dir1'))
        os.makedirs(os.path.join(workspace, 'dir2'))

        # create cyclical symlinks
        os.symlink(os.path.join('..', 'dir1'),
                   os.path.join(workspace, 'dir2', 'ldir1'))
        os.symlink(os.path.join('..', 'dir2'),
                   os.path.join(workspace, 'dir1', 'ldir2'))

        # create one file in each dir
        open(os.path.join(workspace, 'dir1', 'f1.txt'), 'a').close()
        open(os.path.join(workspace, 'dir1', 'ldir2', 'f2.txt'), 'a').close()

        data = []

        def callback(rootdirectory, directory, subdirs, files):
            for f in files:
                data.append(f)

        ManifestParser._walk_directories([workspace], callback)
        self.assertEqual(sorted(data), ['f1.txt', 'f2.txt'])


if __name__ == '__main__':
    mozunit.main()
