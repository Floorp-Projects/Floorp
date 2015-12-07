# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from mozpack.copier import (
    FileCopier,
    FilePurger,
    FileRegistry,
    FileRegistrySubtree,
    Jarrer,
)
from mozpack.files import (
    GeneratedFile,
    ExistingFile,
)
from mozpack.mozjar import JarReader
import mozpack.path as mozpath
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


class BaseTestFileRegistry(MatchTestTemplate):
    def add(self, path):
        self.registry.add(path, GeneratedFile(path))

    def do_check(self, pattern, result):
        self.checked = True
        if result:
            self.assertTrue(self.registry.contains(pattern))
        else:
            self.assertFalse(self.registry.contains(pattern))
        self.assertEqual(self.registry.match(pattern), result)

    def do_test_file_registry(self, registry):
        self.registry = registry
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

    def do_test_registry_paths(self, registry):
        self.registry = registry

        # Can't add a file if it requires a directory in place of a
        # file we also require.
        self.registry.add('foo', GeneratedFile('foo'))
        self.assertRaises(ErrorMessage, self.registry.add, 'foo/bar',
                          GeneratedFile('foobar'))

        # Can't add a file if we already have a directory there.
        self.registry.add('bar/baz', GeneratedFile('barbaz'))
        self.assertRaises(ErrorMessage, self.registry.add, 'bar',
                          GeneratedFile('bar'))

        # Bump the count of things that require bar/ to 2.
        self.registry.add('bar/zot', GeneratedFile('barzot'))
        self.assertRaises(ErrorMessage, self.registry.add, 'bar',
                          GeneratedFile('bar'))

        # Drop the count of things that require bar/ to 1.
        self.registry.remove('bar/baz')
        self.assertRaises(ErrorMessage, self.registry.add, 'bar',
                          GeneratedFile('bar'))

        # Drop the count of things that require bar/ to 0.
        self.registry.remove('bar/zot')
        self.registry.add('bar/zot', GeneratedFile('barzot'))

class TestFileRegistry(BaseTestFileRegistry, unittest.TestCase):
    def test_partial_paths(self):
        cases = {
            'foo/bar/baz/zot': ['foo/bar/baz', 'foo/bar', 'foo'],
            'foo/bar': ['foo'],
            'bar': [],
        }
        reg = FileRegistry()
        for path, parts in cases.iteritems():
            self.assertEqual(reg._partial_paths(path), parts)

    def test_file_registry(self):
        self.do_test_file_registry(FileRegistry())

    def test_registry_paths(self):
        self.do_test_registry_paths(FileRegistry())

    def test_required_directories(self):
        self.registry = FileRegistry()

        self.registry.add('foo', GeneratedFile('foo'))
        self.assertEqual(self.registry.required_directories(), set())

        self.registry.add('bar/baz', GeneratedFile('barbaz'))
        self.assertEqual(self.registry.required_directories(), {'bar'})

        self.registry.add('bar/zot', GeneratedFile('barzot'))
        self.assertEqual(self.registry.required_directories(), {'bar'})

        self.registry.add('bar/zap/zot', GeneratedFile('barzapzot'))
        self.assertEqual(self.registry.required_directories(), {'bar', 'bar/zap'})

        self.registry.remove('bar/zap/zot')
        self.assertEqual(self.registry.required_directories(), {'bar'})

        self.registry.remove('bar/baz')
        self.assertEqual(self.registry.required_directories(), {'bar'})

        self.registry.remove('bar/zot')
        self.assertEqual(self.registry.required_directories(), set())

        self.registry.add('x/y/z', GeneratedFile('xyz'))
        self.assertEqual(self.registry.required_directories(), {'x', 'x/y'})


class TestFileRegistrySubtree(BaseTestFileRegistry, unittest.TestCase):
    def test_file_registry_subtree_base(self):
        registry = FileRegistry()
        self.assertEqual(registry, FileRegistrySubtree('', registry))
        self.assertNotEqual(registry, FileRegistrySubtree('base', registry))

    def create_registry(self):
        registry = FileRegistry()
        registry.add('foo/bar', GeneratedFile('foo/bar'))
        registry.add('baz/qux', GeneratedFile('baz/qux'))
        return FileRegistrySubtree('base/root', registry)

    def test_file_registry_subtree(self):
        self.do_test_file_registry(self.create_registry())

    def test_registry_paths_subtree(self):
        registry = FileRegistry()
        self.do_test_registry_paths(self.create_registry())


class TestFileCopier(TestWithTmpDir):
    def all_dirs(self, base):
        all_dirs = set()
        for root, dirs, files in os.walk(base):
            if not dirs:
                all_dirs.add(mozpath.relpath(root, base))
        return all_dirs

    def all_files(self, base):
        all_files = set()
        for root, dirs, files in os.walk(base):
            for f in files:
                all_files.add(
                    mozpath.join(mozpath.relpath(root, base), f))
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

    def test_symlink_directory_replaced(self):
        """Directory symlinks in destination are replaced if they need to be
        real directories."""
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

    def test_remove_unaccounted_directory_symlinks(self):
        """Directory symlinks in destination that are not in the way are
        deleted according to remove_unaccounted and
        remove_all_directory_symlinks.
        """
        if not self.symlink_supported:
            return

        dest = self.tmppath('dest')

        copier = FileCopier()
        copier.add('foo/bar/baz', GeneratedFile('foobarbaz'))

        os.makedirs(self.tmppath('dest/foo'))
        dummy = self.tmppath('dummy')
        os.mkdir(dummy)

        os.mkdir(self.tmppath('dest/zot'))
        link = self.tmppath('dest/zot/zap')
        os.symlink(dummy, link)

        # If not remove_unaccounted but remove_empty_directories, then
        # the symlinked directory remains (as does its containing
        # directory).
        result = copier.copy(dest, remove_unaccounted=False,
            remove_empty_directories=True,
            remove_all_directory_symlinks=False)

        st = os.lstat(link)
        self.assertTrue(stat.S_ISLNK(st.st_mode))
        self.assertFalse(stat.S_ISDIR(st.st_mode))

        self.assertEqual(self.all_files(dest), set(copier.paths()))
        self.assertEqual(self.all_dirs(dest), set(['foo/bar']))

        self.assertEqual(result.removed_directories, set())
        self.assertEqual(len(result.updated_files), 1)

        # If remove_unaccounted but not remove_empty_directories, then
        # only the symlinked directory is removed.
        result = copier.copy(dest, remove_unaccounted=True,
            remove_empty_directories=False,
            remove_all_directory_symlinks=False)

        st = os.lstat(self.tmppath('dest/zot'))
        self.assertFalse(stat.S_ISLNK(st.st_mode))
        self.assertTrue(stat.S_ISDIR(st.st_mode))

        self.assertEqual(result.removed_files, set([link]))
        self.assertEqual(result.removed_directories, set())

        self.assertEqual(self.all_files(dest), set(copier.paths()))
        self.assertEqual(self.all_dirs(dest), set(['foo/bar', 'zot']))

        # If remove_unaccounted and remove_empty_directories, then
        # both the symlink and its containing directory are removed.
        link = self.tmppath('dest/zot/zap')
        os.symlink(dummy, link)

        result = copier.copy(dest, remove_unaccounted=True,
            remove_empty_directories=True,
            remove_all_directory_symlinks=False)

        self.assertEqual(result.removed_files, set([link]))
        self.assertEqual(result.removed_directories, set([self.tmppath('dest/zot')]))

        self.assertEqual(self.all_files(dest), set(copier.paths()))
        self.assertEqual(self.all_dirs(dest), set(['foo/bar']))

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
        self.assertEqual(self.all_dirs(self.tmpdir), set(['populateddir']))
        self.assertEqual(result.removed_files, set())
        self.assertEqual(result.removed_directories,
            set([self.tmppath('emptydir')]))

    def test_no_remove_empty_directories(self):
        copier = FileCopier()
        copier.add('foo', GeneratedFile('foo'))

        with open(self.tmppath('bar'), 'a'):
            pass

        os.mkdir(self.tmppath('emptydir'))
        d = self.tmppath('populateddir')
        os.mkdir(d)

        with open(self.tmppath('populateddir/foo'), 'a'):
            pass

        result = copier.copy(self.tmpdir, remove_unaccounted=False,
            remove_empty_directories=False)

        self.assertEqual(self.all_files(self.tmpdir), set(['foo', 'bar',
            'populateddir/foo']))
        self.assertEqual(self.all_dirs(self.tmpdir), set(['emptydir',
            'populateddir']))
        self.assertEqual(result.removed_files, set())
        self.assertEqual(result.removed_directories, set())

    def test_optional_exists_creates_unneeded_directory(self):
        """Demonstrate that a directory not strictly required, but specified
        as the path to an optional file, will be unnecessarily created.

        This behaviour is wrong; fixing it is tracked by Bug 972432;
        and this test exists to guard against unexpected changes in
        behaviour.
        """

        dest = self.tmppath('dest')

        copier = FileCopier()
        copier.add('foo/bar', ExistingFile(required=False))

        result = copier.copy(dest)

        st = os.lstat(self.tmppath('dest/foo'))
        self.assertFalse(stat.S_ISLNK(st.st_mode))
        self.assertTrue(stat.S_ISDIR(st.st_mode))

        # What's worse, we have no record that dest was created.
        self.assertEquals(len(result.updated_files), 0)

        # But we do have an erroneous record of an optional file
        # existing when it does not.
        self.assertIn(self.tmppath('dest/foo/bar'), result.existing_files)

    def test_remove_unaccounted_file_registry(self):
        """Test FileCopier.copy(remove_unaccounted=FileRegistry())"""

        dest = self.tmppath('dest')

        copier = FileCopier()
        copier.add('foo/bar/baz', GeneratedFile('foobarbaz'))
        copier.add('foo/bar/qux', GeneratedFile('foobarqux'))
        copier.add('foo/hoge/fuga', GeneratedFile('foohogefuga'))
        copier.add('foo/toto/tata', GeneratedFile('footototata'))

        os.makedirs(os.path.join(dest, 'bar'))
        with open(os.path.join(dest, 'bar', 'bar'), 'w') as fh:
            fh.write('barbar');
        os.makedirs(os.path.join(dest, 'foo', 'toto'))
        with open(os.path.join(dest, 'foo', 'toto', 'toto'), 'w') as fh:
            fh.write('foototototo');

        result = copier.copy(dest, remove_unaccounted=False)

        self.assertEqual(self.all_files(dest),
                         set(copier.paths()) | { 'foo/toto/toto', 'bar/bar'})
        self.assertEqual(self.all_dirs(dest),
                         {'foo/bar', 'foo/hoge', 'foo/toto', 'bar'})

        copier2 = FileCopier()
        copier2.add('foo/hoge/fuga', GeneratedFile('foohogefuga'))

        # We expect only files copied from the first copier to be removed,
        # not the extra file that was there beforehand.
        result = copier2.copy(dest, remove_unaccounted=copier)

        self.assertEqual(self.all_files(dest),
                         set(copier2.paths()) | { 'foo/toto/toto', 'bar/bar'})
        self.assertEqual(self.all_dirs(dest),
                         {'foo/hoge', 'foo/toto', 'bar'})
        self.assertEqual(result.updated_files,
                         {self.tmppath('dest/foo/hoge/fuga')})
        self.assertEqual(result.existing_files, set())
        self.assertEqual(result.removed_files, {self.tmppath(p) for p in
            ('dest/foo/bar/baz', 'dest/foo/bar/qux', 'dest/foo/toto/tata')})
        self.assertEqual(result.removed_directories,
                         {self.tmppath('dest/foo/bar')})


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
