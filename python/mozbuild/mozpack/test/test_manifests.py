# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import unicode_literals

import os

import mozunit

from mozpack.copier import (
    FileCopier,
    FileRegistry,
)
from mozpack.manifests import (
    InstallManifest,
)
from mozpack.test.test_files import TestWithTmpDir


class TestInstallManifest(TestWithTmpDir):
    def test_construct(self):
        m = InstallManifest()
        self.assertEqual(len(m), 0)

    def test_adds(self):
        m = InstallManifest()
        m.add_symlink('s_source', 's_dest')
        m.add_copy('c_source', 'c_dest')
        m.add_required_exists('e_dest')
        m.add_optional_exists('o_dest')
        m.add_pattern_symlink('ps_base', 'ps/*', 'ps_dest')
        m.add_pattern_copy('pc_base', 'pc/**', 'pc_dest')

        self.assertEqual(len(m), 6)
        self.assertIn('s_dest', m)
        self.assertIn('c_dest', m)
        self.assertIn('e_dest', m)
        self.assertIn('o_dest', m)

        with self.assertRaises(ValueError):
            m.add_symlink('s_other', 's_dest')

        with self.assertRaises(ValueError):
            m.add_copy('c_other', 'c_dest')

        with self.assertRaises(ValueError):
            m.add_required_exists('e_dest')

        with self.assertRaises(ValueError):
            m.add_optional_exists('o_dest')

        with self.assertRaises(ValueError):
            m.add_pattern_symlink('ps_base', 'ps/*', 'ps_dest')

        with self.assertRaises(ValueError):
            m.add_pattern_copy('pc_base', 'pc/**', 'pc_dest')

    def _get_test_manifest(self):
        m = InstallManifest()
        m.add_symlink(self.tmppath('s_source'), 's_dest')
        m.add_copy(self.tmppath('c_source'), 'c_dest')
        m.add_required_exists('e_dest')
        m.add_optional_exists('o_dest')
        m.add_pattern_symlink('ps_base', '*', 'ps_dest')
        m.add_pattern_copy('pc_base', '**', 'pc_dest')

        return m

    def test_serialization(self):
        m = self._get_test_manifest()

        p = self.tmppath('m')
        m.write(path=p)
        self.assertTrue(os.path.isfile(p))

        with open(p, 'rb') as fh:
            c = fh.read()

        self.assertEqual(c.count('\n'), 7)

        lines = c.splitlines()
        self.assertEqual(len(lines), 7)

        self.assertEqual(lines[0], '3')

        m2 = InstallManifest(path=p)
        self.assertEqual(m, m2)
        p2 = self.tmppath('m2')
        m2.write(path=p2)

        with open(p2, 'rb') as fh:
            c2 = fh.read()

        self.assertEqual(c, c2)

    def test_populate_registry(self):
        m = self._get_test_manifest()
        r = FileRegistry()
        m.populate_registry(r)

        self.assertEqual(len(r), 4)
        self.assertEqual(r.paths(), ['c_dest', 'e_dest', 'o_dest', 's_dest'])

    def test_pattern_expansion(self):
        source = self.tmppath('source')
        os.mkdir(source)
        os.mkdir('%s/base' % source)
        os.mkdir('%s/base/foo' % source)

        with open('%s/base/foo/file1' % source, 'a'):
            pass

        with open('%s/base/foo/file2' % source, 'a'):
            pass

        m = InstallManifest()
        m.add_pattern_symlink('%s/base' % source, '**', 'dest')

        c = FileCopier()
        m.populate_registry(c)
        self.assertEqual(c.paths(), ['dest/foo/file1', 'dest/foo/file2'])

    def test_or(self):
        m1 = self._get_test_manifest()
        orig_length = len(m1)
        m2 = InstallManifest()
        m2.add_symlink('s_source2', 's_dest2')
        m2.add_copy('c_source2', 'c_dest2')

        m1 |= m2

        self.assertEqual(len(m2), 2)
        self.assertEqual(len(m1), orig_length + 2)

        self.assertIn('s_dest2', m1)
        self.assertIn('c_dest2', m1)

    def test_copier_application(self):
        dest = self.tmppath('dest')
        os.mkdir(dest)

        to_delete = self.tmppath('dest/to_delete')
        with open(to_delete, 'a'):
            pass

        with open(self.tmppath('s_source'), 'wt') as fh:
            fh.write('symlink!')

        with open(self.tmppath('c_source'), 'wt') as fh:
            fh.write('copy!')

        with open(self.tmppath('dest/e_dest'), 'a'):
            pass

        with open(self.tmppath('dest/o_dest'), 'a'):
            pass

        m = self._get_test_manifest()
        c = FileCopier()
        m.populate_registry(c)
        result = c.copy(dest)

        self.assertTrue(os.path.exists(self.tmppath('dest/s_dest')))
        self.assertTrue(os.path.exists(self.tmppath('dest/c_dest')))
        self.assertTrue(os.path.exists(self.tmppath('dest/e_dest')))
        self.assertTrue(os.path.exists(self.tmppath('dest/o_dest')))
        self.assertFalse(os.path.exists(to_delete))

        with open(self.tmppath('dest/s_dest'), 'rt') as fh:
            self.assertEqual(fh.read(), 'symlink!')

        with open(self.tmppath('dest/c_dest'), 'rt') as fh:
            self.assertEqual(fh.read(), 'copy!')

        self.assertEqual(result.updated_files, set(self.tmppath(p) for p in (
            'dest/s_dest', 'dest/c_dest')))
        self.assertEqual(result.existing_files,
            set([self.tmppath('dest/e_dest'), self.tmppath('dest/o_dest')]))
        self.assertEqual(result.removed_files, {to_delete})
        self.assertEqual(result.removed_directories, set())




if __name__ == '__main__':
    mozunit.main()
