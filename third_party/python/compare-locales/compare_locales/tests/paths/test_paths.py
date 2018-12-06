# -*- coding: utf-8 -*-
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import
import unittest

from compare_locales.paths import File


class TestFile(unittest.TestCase):
    def test_hash_and_equality(self):
        f1 = File('/tmp/full/path/to/file', 'path/to/file')
        d = {}
        d[f1] = True
        self.assertIn(f1, d)
        f2 = File('/tmp/full/path/to/file', 'path/to/file')
        self.assertIn(f2, d)
        f2 = File('/tmp/full/path/to/file', 'path/to/file', locale='en')
        self.assertNotIn(f2, d)
        # trigger hash collisions between File and non-File objects
        self.assertEqual(hash(f1), hash(f1.localpath))
        self.assertNotIn(f1.localpath, d)
        f1 = File('/tmp/full/other/path', 'other/path')
        d[f1.localpath] = True
        self.assertIn(f1.localpath, d)
        self.assertNotIn(f1, d)
