#!/usr/bin/env python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import shutil
import tempfile
import unittest

from manifestparser import convert

class TestSymlinkConversion(unittest.TestCase):
    """
    test conversion of a directory tree with symlinks to a manifest structure
    """

    # Currently broken: see
    # https://bugzilla.mozilla.org/show_bug.cgi?id=902610
    # https://bugzilla.mozilla.org/show_bug.cgi?id=920938

    def create_stub(self, directory=None):
        """stub out a directory with files in it"""

        files = ('foo', 'bar', 'fleem')
        if directory is None:
            directory = tempfile.mkdtemp()
        for i in files:
            file(os.path.join(directory, i), 'w').write(i)
        subdir = os.path.join(directory, 'subdir')
        os.mkdir(subdir)
        file(os.path.join(subdir, 'subfile'), 'w').write('baz')
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
        except:
            raise
        finally:
            shutil.rmtree(stub)
            os.chdir(oldcwd)

    def test_relpath_symlink(self):
        """
        Ensure `relative_to` works in a symlink.
        Not available on windows.
        """

        symlink = getattr(os, 'symlink', None)
        if symlink is None:
            return # symlinks unavailable on this platform

        oldcwd = os.getcwd()
        workspace = tempfile.mkdtemp()
        try:
            tmpdir = os.path.join(workspace, 'directory')
            os.makedirs(tmpdir)
            linkdir = os.path.join(workspace, 'link')
            symlink(tmpdir, linkdir)
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
            symlink(tmpdir, linkdir)
            self.create_stub(tmpdir)
            files = ['../bar', '../fleem', '../foo', 'subfile']
            subdir = os.path.join(linkdir, 'subdir')
            subsubdir = os.path.join(subdir, 'sub')
            os.makedirs(subsubdir)
            linksubdir = os.path.join(linkdir, 'linky')
            linksubsubdir = os.path.join(subsubdir, 'linky')
            symlink(subdir, linksubdir)
            symlink(subdir, linksubsubdir)
            for dest in (subdir,):
                os.chdir(dest)
                for directory in (tmpdir, linkdir):
                    parser = convert([directory], relative_to='.')
                    self.assertEqual([i['name'] for i in parser.tests],
                                     files)
        finally:
            shutil.rmtree(workspace)
            os.chdir(oldcwd)


if __name__ == '__main__':
    unittest.main()
