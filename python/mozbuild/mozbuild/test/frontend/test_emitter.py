# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import unicode_literals

import os
import unittest

from mozbuild.frontend.data import DirectoryTraversal
from mozbuild.frontend.emitter import TreeMetadataEmitter
from mozbuild.frontend.reader import BuildReader

from mozbuild.test.common import MockConfig


data_path = os.path.abspath(os.path.dirname(__file__))
data_path = os.path.join(data_path, 'data')


class TestEmitterBasic(unittest.TestCase):
    def reader(self, name):
        config = MockConfig(os.path.join(data_path, name))
        config.substs['ENABLE_TESTS'] = '1'

        return BuildReader(config)

    def test_dirs_traversal_simple(self):
        reader = self.reader('traversal-simple')
        emitter = TreeMetadataEmitter(reader.config)

        objs = list(emitter.emit(reader.read_topsrcdir()))

        self.assertEqual(len(objs), 4)

        for o in objs:
            self.assertIsInstance(o, DirectoryTraversal)
            self.assertEqual(o.parallel_dirs, [])
            self.assertEqual(o.tool_dirs, [])
            self.assertEqual(o.test_dirs, [])
            self.assertEqual(o.test_tool_dirs, [])
            self.assertEqual(len(o.tier_dirs), 0)
            self.assertEqual(len(o.tier_static_dirs), 0)

        reldirs = [o.relativedir for o in objs]
        self.assertEqual(reldirs, ['', 'foo', 'foo/biz', 'bar'])

        dirs = [o.dirs for o in objs]
        self.assertEqual(dirs, [['foo', 'bar'], ['biz'], [], []])

    def test_traversal_all_vars(self):
        reader = self.reader('traversal-all-vars')
        emitter = TreeMetadataEmitter(reader.config)

        objs = list(emitter.emit(reader.read_topsrcdir()))
        self.assertEqual(len(objs), 6)

        for o in objs:
            self.assertIsInstance(o, DirectoryTraversal)

        reldirs = set([o.relativedir for o in objs])
        self.assertEqual(reldirs, set(['', 'parallel', 'regular', 'test',
            'test_tool', 'tool']))

        for o in objs:
            reldir = o.relativedir

            if reldir == '':
                self.assertEqual(o.dirs, ['regular'])
                self.assertEqual(o.parallel_dirs, ['parallel'])
                self.assertEqual(o.test_dirs, ['test'])
                self.assertEqual(o.test_tool_dirs, ['test_tool'])
                self.assertEqual(o.tool_dirs, ['tool'])

    def test_tier_simple(self):
        reader = self.reader('traversal-tier-simple')
        emitter = TreeMetadataEmitter(reader.config)

        objs = list(emitter.emit(reader.read_topsrcdir()))
        self.assertEqual(len(objs), 6)

        reldirs = [o.relativedir for o in objs]
        self.assertEqual(reldirs, ['', 'foo', 'foo/biz', 'foo_static', 'bar',
            'baz'])

