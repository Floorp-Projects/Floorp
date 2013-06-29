# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import unicode_literals

import os
import unittest

import mozunit

from mozpack.manifests import (
    PurgeManifest,
    UnreadablePurgeManifest,
)
from mozpack.test.test_files import TestWithTmpDir


class TestPurgeManifest(TestWithTmpDir):
    def test_construct(self):
        m = PurgeManifest()
        self.assertEqual(m.relpath, '')
        self.assertEqual(len(m.entries), 0)

    def test_serialization(self):
        m = PurgeManifest(relpath='rel')
        m.add('foo')
        m.add('bar')
        p = self.tmppath('m')
        m.write_file(p)

        self.assertTrue(os.path.exists(p))

        m2 = PurgeManifest.from_path(p)
        self.assertEqual(m.relpath, m2.relpath)
        self.assertEqual(m.entries, m2.entries)
        self.assertEqual(m, m2)

    def test_unknown_version(self):
        p = self.tmppath('bad')

        with open(p, 'wt') as fh:
            fh.write('2\n')
            fh.write('not relevant')

        with self.assertRaises(UnreadablePurgeManifest):
            PurgeManifest.from_path(p)

