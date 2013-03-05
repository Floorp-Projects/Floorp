# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import unicode_literals

import hashlib
import os
import unittest

from mozfile.mozfile import NamedTemporaryFile
from mozunit import (
    main,
    MockedOpen,
)

from mozbuild.util import (
    FileAvoidWrite,
    hash_file,
    resolve_target_to_make,
)


data_path = os.path.abspath(os.path.dirname(__file__))
data_path = os.path.join(data_path, 'data')


class TestHashing(unittest.TestCase):
    def test_hash_file_known_hash(self):
        """Ensure a known hash value is recreated."""
        data = b'The quick brown fox jumps over the lazy cog'
        expected = 'de9f2c7fd25e1b3afad3e85a0bd17d9b100db4b3'

        temp = NamedTemporaryFile()
        temp.write(data)
        temp.flush()

        actual = hash_file(temp.name)

        self.assertEqual(actual, expected)

    def test_hash_file_large(self):
        """Ensure that hash_file seems to work with a large file."""
        data = b'x' * 1048576

        hasher = hashlib.sha1()
        hasher.update(data)
        expected = hasher.hexdigest()

        temp = NamedTemporaryFile()
        temp.write(data)
        temp.flush()

        actual = hash_file(temp.name)

        self.assertEqual(actual, expected)


class TestFileAvoidWrite(unittest.TestCase):
    def test_file_avoid_write(self):
        with MockedOpen({'file': 'content'}):
            # Overwriting an existing file replaces its content
            faw = FileAvoidWrite('file')
            faw.write('bazqux')
            self.assertEqual(faw.close(), (True, True))
            self.assertEqual(open('file', 'r').read(), 'bazqux')

            # Creating a new file (obviously) stores its content
            faw = FileAvoidWrite('file2')
            faw.write('content')
            self.assertEqual(faw.close(), (False, True))
            self.assertEqual(open('file2').read(), 'content')

        with MockedOpen({'file': 'content'}):
            with FileAvoidWrite('file') as file:
                file.write('foobar')

            self.assertEqual(open('file', 'r').read(), 'foobar')

        class MyMockedOpen(MockedOpen):
            '''MockedOpen extension to raise an exception if something
            attempts to write in an opened file.
            '''
            def __call__(self, name, mode):
                if 'w' in mode:
                    raise Exception, 'Unexpected open with write mode'
                return MockedOpen.__call__(self, name, mode)

        with MyMockedOpen({'file': 'content'}):
            # Validate that MyMockedOpen works as intended
            file = FileAvoidWrite('file')
            file.write('foobar')
            self.assertRaises(Exception, file.close)

            # Check that no write actually happens when writing the
            # same content as what already is in the file
            faw = FileAvoidWrite('file')
            faw.write('content')
            self.assertEqual(faw.close(), (True, False))


class TestResolveTargetToMake(unittest.TestCase):
    def setUp(self):
        self.topobjdir = data_path

    def assertResolve(self, path, expected):
        self.assertEqual(resolve_target_to_make(self.topobjdir, path), expected)

    def test_absolute_path(self):
        abspath = os.path.abspath(os.path.join(self.topobjdir, 'test-dir'))
        self.assertResolve(abspath, (None, None))

    def test_dir(self):
        self.assertResolve('test-dir', ('test-dir', None))
        self.assertResolve('test-dir/with', ('test-dir/with', None))
        self.assertResolve('test-dir/with', ('test-dir/with', None))
        self.assertResolve('test-dir/without', ('test-dir', None))
        self.assertResolve('test-dir/without/with', ('test-dir/without/with', None))

    def test_top_level(self):
        self.assertResolve('package', (None, 'package'))

    def test_regular_file(self):
        self.assertResolve('test-dir/with/file', ('test-dir/with', 'file'))
        self.assertResolve('test-dir/with/without/file', ('test-dir/with', 'without/file'))
        self.assertResolve('test-dir/with/without/with/file', ('test-dir/with/without/with', 'file'))

        self.assertResolve('test-dir/without/file', ('test-dir', 'without/file'))
        self.assertResolve('test-dir/without/with/file', ('test-dir/without/with', 'file'))
        self.assertResolve('test-dir/without/with/without/file', ('test-dir/without/with', 'without/file'))


if __name__ == '__main__':
    main()
