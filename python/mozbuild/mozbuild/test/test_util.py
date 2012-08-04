# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import hashlib
import unittest

from StringIO import StringIO
from tempfile import NamedTemporaryFile

from mozbuild.util import hash_file


class TestHashing(unittest.TestCase):
    def test_hash_file_known_hash(self):
        """Ensure a known hash value is recreated."""
        data = 'The quick brown fox jumps over the lazy cog'
        expected = 'de9f2c7fd25e1b3afad3e85a0bd17d9b100db4b3'

        temp = NamedTemporaryFile()
        temp.write(data)
        temp.flush()

        actual = hash_file(temp.name)

        self.assertEqual(actual, expected)

    def test_hash_file_large(self):
        """Ensure that hash_file seems to work with a large file."""
        data = 'x' * 1048576

        hasher = hashlib.sha1()
        hasher.update(data)
        expected = hasher.hexdigest()

        temp = NamedTemporaryFile()
        temp.write(data)
        temp.flush()

        actual = hash_file(temp.name)

        self.assertEqual(actual, expected)

