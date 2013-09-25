# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from mozpack.copier import (
    FileCopier,
    FilePurger,
    FileRegistry,
    Jarrer,
)
from mozpack.files import GeneratedFile
from mozpack.mozjar import JarReader
import mozpack.path
import unittest
import mozunit
import os
import stat
from mozpack.errors import ErrorMessage
from mozpack.test.test_files import (
    MockDest,
    MatchTestTemplate,
    TestWithTmpDir,
)


class TestFileRegistry(MatchTestTemplate, unittest.TestCase):
    def add(self, path):
        self.registry.add(path, GeneratedFile(path))

    def do_check(self, pattern, result):
        self.checked = True
        if result:
            self.assertTrue(self.registry.contains(pattern))
        else:
            self.assertFalse(self.registry.contains(pattern))
        self.assertEqual(self.registry.match(pattern), result)

    def test_file_registry(self):
        self.registry = FileRegistry()
        self.registry.add('foo', GeneratedFile('foo'))
        bar = GeneratedFile('bar')
        self.registry.add('bar', bar)
        self.assertEqual(self.registry.paths(), ['foo', 'bar'])
        self.assertEqual(self.registry['bar'], bar)

        self.assertRaises(ErrorMessage, self.registry.add, 'foo',
                          GeneratedFile('foo2'))

        self.assertRaises(ErrorMessage, self.registry.remove, 'qux')

        self.assertRaises(ErrorMessage, self.registry.add, 'foo/bar',
                          GeneratedFile('foobar'))
        self.assertRaises(ErrorMessage, self.registry.add, 'foo/bar/baz',
                          GeneratedFile('foobar'))

        self.assertEqual(self.registry.paths(), ['foo', 'bar'])

        self.registry.remove('foo')
        self.assertEqual(self.registry.paths(), ['bar'])
        self.registry.remove('bar')
        self.assertEqual(self.registry.paths(), [])

        self.prepare_match_test()
        self.do_match_test()
        self.assertTrue(self.checked)
        self.assertEqual(self.registry.paths(), [
            'bar',
            'foo/bar',
            'foo/baz',
            'foo/qux/1',
            'foo/qux/bar',
            'foo/qux/2/test',
            'foo/qux/2/test2',
        ])

        self.registry.remove('foo/qux')
        self.assertEqual(self.registry.paths(), ['bar', 'foo/bar', 'foo/baz'])

        self.registry.add('foo/qux', GeneratedFile('fooqux'))
        self.assertEqual(self.registry.paths(), ['bar', 'foo/bar', 'foo/baz',
                                                 'foo/qux'])
        self.registry.remove('foo/b*')
        self.assertEqual(self.registry.paths(), ['bar', 'foo/qux'])

        self.assertEqual([f for f, c in self.registry], ['bar', 'foo/qux'])
        self.assertEqual(len(self.registry), 2)

        self.add('foo/.foo')
        self.assertTrue(self.registry.contains('foo/.foo'))


class TestFileCopier(TestWithTmpDir):
    def all_dirs(self, base):
        all_dirs = set()
        for root, dirs, files in os.walk(base):
            if not dirs:
                all_dirs.add(mozpack.path.relpath(root, base))
        return all_dirs

    def all_files(self, base):
        all_files = set()
        for root, dirs, files in os.walk(base):
            for f in files:
                all_files.add(
                    mozpack.path.join(mozpack.path.relpath(root, base), f))
        return all_files

    def test_file_copier(self):
        copier = FileCopier()
        copier.add('foo/bar', GeneratedFile('foobar'))
        copier.add('foo/qux', GeneratedFile('fooqux'))
        copier.add('foo/deep/nested/directory/file', GeneratedFile('fooz'))
        copier.add('bar', GeneratedFile('bar'))
        copier.add('qux/foo', GeneratedFile('quxfoo'))
        copier.add('qux/bar', GeneratedFile(''))

        result = copier.copy(self.tmpdir)
        self.assertEqual(self.all_files(self.tmpdir), set(copier.paths()))
        self.assertEqual(self.all_dirs(self.tmpdir),
                         set(['foo/deep/nested/directory', 'qux']))

        self.assertEqual(result.updated_files, set(self.tmppath(p) for p in
            self.all_files(self.tmpdir)))
        self.assertEqual(result.existing_files, set())
        self.assertEqual(result.removed_files, set())
        self.assertEqual(result.removed_directories, set())

        copier.remove('foo')
        copier.add('test', GeneratedFile('test'))
        result = copier.copy(self.tmpdir)
        self.assertEqual(self.all_files(self.tmpdir), set(copier.paths()))
        self.assertEqual(self.all_dirs(self.tmpdir), set(['qux']))
        self.assertEqual(result.removed_files, set(self.tmppath(p) for p in
            ('foo/bar', 'foo/qux', 'foo/deep/nested/directory/file')))

    def test_symlink_directory(self):
        """Directory symlinks in destination are deleted."""
        if not self.symlink_supported:
            return

        dest = self.tmppath('dest')

        copier = FileCopier()
        copier.add('foo/bar/baz', GeneratedFile('foobarbaz'))

        os.makedirs(self.tmppath('dest/foo'))
        dummy = self.tmppath('dummy')
        os.mkdir(dummy)
        link = self.tmppath('dest/foo/bar')
        os.symlink(dummy, link)

        result = copier.copy(dest)

        st = os.lstat(link)
        self.assertFalse(stat.S_ISLNK(st.st_mode))
        self.assertTrue(stat.S_ISDIR(st.st_mode))

        self.assertEqual(self.all_files(dest), set(copier.paths()))

        self.assertEqual(result.removed_directories, set())
        self.assertEqual(len(result.updated_files), 1)

    def test_permissions(self):
        """Ensure files without write permission can be deleted."""
        with open(self.tmppath('dummy'), 'a'):
            pass

        p = self.tmppath('no_perms')
        with open(p, 'a'):
            pass

        # Make file and directory unwritable. Reminder: making a directory
        # unwritable prevents modifications (including deletes) from the list
        # of files in that directory.
        os.chmod(p, 0400)
        os.chmod(self.tmpdir, 0400)

        copier = FileCopier()
        copier.add('dummy', GeneratedFile('content'))
        result = copier.copy(self.tmpdir)
        self.assertEqual(result.removed_files_count, 1)
        self.assertFalse(os.path.exists(p))

    def test_no_remove(self):
        copier = FileCopier()
        copier.add('foo', GeneratedFile('foo'))

        with open(self.tmppath('bar'), 'a'):
            pass

        os.mkdir(self.tmppath('emptydir'))
        d = self.tmppath('populateddir')
        os.mkdir(d)

        with open(self.tmppath('populateddir/foo'), 'a'):
            pass

        result = copier.copy(self.tmpdir, remove_unaccounted=False)

        self.assertEqual(self.all_files(self.tmpdir), set(['foo', 'bar',
            'populateddir/foo']))
        self.assertEqual(result.removed_files, set())
        self.assertEqual(result.removed_directories,
            set([self.tmppath('emptydir')]))


class TestFilePurger(TestWithTmpDir):
    def test_file_purger(self):
        existing = os.path.join(self.tmpdir, 'existing')
        extra = os.path.join(self.tmpdir, 'extra')
        empty_dir = os.path.join(self.tmpdir, 'dir')

        with open(existing, 'a'):
            pass

        with open(extra, 'a'):
            pass

        os.mkdir(empty_dir)
        with open(os.path.join(empty_dir, 'foo'), 'a'):
            pass

        self.assertTrue(os.path.exists(existing))
        self.assertTrue(os.path.exists(extra))

        purger = FilePurger()
        purger.add('existing')
        result = purger.purge(self.tmpdir)
        self.assertEqual(result.removed_files, set(self.tmppath(p) for p in
            ('extra', 'dir/foo')))
        self.assertEqual(result.removed_files_count, 2)
        self.assertEqual(result.removed_directories_count, 1)

        self.assertTrue(os.path.exists(existing))
        self.assertFalse(os.path.exists(extra))
        self.assertFalse(os.path.exists(empty_dir))


class TestJarrer(unittest.TestCase):
    def check_jar(self, dest, copier):
        jar = JarReader(fileobj=dest)
        self.assertEqual([f.filename for f in jar], copier.paths())
        for f in jar:
            self.assertEqual(f.uncompressed_data.read(),
                             copier[f.filename].content)

    def test_jarrer(self):
        copier = Jarrer()
        copier.add('foo/bar', GeneratedFile('foobar'))
        copier.add('foo/qux', GeneratedFile('fooqux'))
        copier.add('foo/deep/nested/directory/file', GeneratedFile('fooz'))
        copier.add('bar', GeneratedFile('bar'))
        copier.add('qux/foo', GeneratedFile('quxfoo'))
        copier.add('qux/bar', GeneratedFile(''))

        dest = MockDest()
        copier.copy(dest)
        self.check_jar(dest, copier)

        copier.remove('foo')
        copier.add('test', GeneratedFile('test'))
        copier.copy(dest)
        self.check_jar(dest, copier)

        copier.remove('test')
        copier.add('test', GeneratedFile('replaced-content'))
        copier.copy(dest)
        self.check_jar(dest, copier)

        copier.copy(dest)
        self.check_jar(dest, copier)

        preloaded = ['qux/bar', 'bar']
        copier.preload(preloaded)
        copier.copy(dest)

        dest.seek(0)
        jar = JarReader(fileobj=dest)
        self.assertEqual([f.filename for f in jar], preloaded +
                         [p for p in copier.paths() if not p in preloaded])
        self.assertEqual(jar.last_preloaded, preloaded[-1])

if __name__ == '__main__':
    mozunit.main()
