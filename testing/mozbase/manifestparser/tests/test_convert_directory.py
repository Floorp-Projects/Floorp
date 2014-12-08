#!/usr/bin/env python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import shutil
import tempfile
import unittest

from manifestparser import convert
from manifestparser import ManifestParser

here = os.path.dirname(os.path.abspath(__file__))

# In some cases tempfile.mkdtemp() may returns a path which contains
# symlinks. Some tests here will then break, as the manifestparser.convert
# function returns paths that does not contains symlinks.
#
# Workaround is to use the following function, if absolute path of temp dir
# must be compared.
def create_realpath_tempdir():
    """
    Create a tempdir without symlinks.
    """
    return os.path.realpath(tempfile.mkdtemp())

class TestDirectoryConversion(unittest.TestCase):
    """test conversion of a directory tree to a manifest structure"""

    def create_stub(self, directory=None):
        """stub out a directory with files in it"""

        files = ('foo', 'bar', 'fleem')
        if directory is None:
            directory = create_realpath_tempdir()
        for i in files:
            file(os.path.join(directory, i), 'w').write(i)
        subdir = os.path.join(directory, 'subdir')
        os.mkdir(subdir)
        file(os.path.join(subdir, 'subfile'), 'w').write('baz')
        return directory

    def test_directory_to_manifest(self):
        """
        Test our ability to convert a static directory structure to a
        manifest.
        """

        # create a stub directory
        stub = self.create_stub()
        try:
            stub = stub.replace(os.path.sep, "/")
            self.assertTrue(os.path.exists(stub) and os.path.isdir(stub))

            # Make a manifest for it
            manifest = convert([stub])
            self.assertEqual(str(manifest),
"""[%(stub)s/bar]
subsuite = 

[%(stub)s/fleem]
subsuite = 

[%(stub)s/foo]
subsuite = 

[%(stub)s/subdir/subfile]
subsuite = 

""" % dict(stub=stub))
        except:
            raise
        finally:
            shutil.rmtree(stub) # cleanup

    def test_convert_directory_manifests_in_place(self):
        """
        keep the manifests in place
        """

        stub = self.create_stub()
        try:
            ManifestParser.populate_directory_manifests([stub], filename='manifest.ini')
            self.assertEqual(sorted(os.listdir(stub)),
                             ['bar', 'fleem', 'foo', 'manifest.ini', 'subdir'])
            parser = ManifestParser()
            parser.read(os.path.join(stub, 'manifest.ini'))
            self.assertEqual([i['name'] for i in parser.tests],
                             ['subfile', 'bar', 'fleem', 'foo'])
            parser = ManifestParser()
            parser.read(os.path.join(stub, 'subdir', 'manifest.ini'))
            self.assertEqual(len(parser.tests), 1)
            self.assertEqual(parser.tests[0]['name'], 'subfile')
        except:
            raise
        finally:
            shutil.rmtree(stub)

    def test_manifest_ignore(self):
        """test manifest `ignore` parameter for ignoring directories"""

        stub = self.create_stub()
        try:
            ManifestParser.populate_directory_manifests([stub], filename='manifest.ini', ignore=('subdir',))
            parser = ManifestParser()
            parser.read(os.path.join(stub, 'manifest.ini'))
            self.assertEqual([i['name'] for i in parser.tests],
                             ['bar', 'fleem', 'foo'])
            self.assertFalse(os.path.exists(os.path.join(stub, 'subdir', 'manifest.ini')))
        except:
            raise
        finally:
            shutil.rmtree(stub)

    def test_pattern(self):
        """test directory -> manifest with a file pattern"""

        stub = self.create_stub()
        try:
            parser = convert([stub], pattern='f*', relative_to=stub)
            self.assertEqual([i['name'] for i in parser.tests],
                             ['fleem', 'foo'])

            # test multiple patterns
            parser = convert([stub], pattern=('f*', 's*'), relative_to=stub)
            self.assertEqual([i['name'] for i in parser.tests],
                             ['fleem', 'foo', 'subdir/subfile'])
        except:
            raise
        finally:
            shutil.rmtree(stub)

    def test_update(self):
        """
        Test our ability to update tests from a manifest and a directory of
        files
        """

        # boilerplate
        tempdir = create_realpath_tempdir()
        for i in range(10):
            file(os.path.join(tempdir, str(i)), 'w').write(str(i))

        # otherwise empty directory with a manifest file
        newtempdir = create_realpath_tempdir()
        manifest_file = os.path.join(newtempdir, 'manifest.ini')
        manifest_contents = str(convert([tempdir], relative_to=tempdir))
        with file(manifest_file, 'w') as f:
            f.write(manifest_contents)

        # get the manifest
        manifest = ManifestParser(manifests=(manifest_file,))

        # All of the tests are initially missing:
        paths = [str(i) for i in range(10)]
        self.assertEqual([i['name'] for i in manifest.missing()],
                         paths)

        # But then we copy one over:
        self.assertEqual(manifest.get('name', name='1'), ['1'])
        manifest.update(tempdir, name='1')
        self.assertEqual(sorted(os.listdir(newtempdir)),
                        ['1', 'manifest.ini'])

        # Update that one file and copy all the "tests":
        file(os.path.join(tempdir, '1'), 'w').write('secret door')
        manifest.update(tempdir)
        self.assertEqual(sorted(os.listdir(newtempdir)),
                        ['0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'manifest.ini'])
        self.assertEqual(file(os.path.join(newtempdir, '1')).read().strip(),
                        'secret door')

        # clean up:
        shutil.rmtree(tempdir)
        shutil.rmtree(newtempdir)


if __name__ == '__main__':
    unittest.main()
