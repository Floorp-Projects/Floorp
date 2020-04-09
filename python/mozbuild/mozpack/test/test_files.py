# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

from mozbuild.util import (
    ensureParentDir,
    ensure_bytes,
)

from mozpack.errors import (
    ErrorMessage,
    errors,
)
from mozpack.files import (
    AbsoluteSymlinkFile,
    ComposedFinder,
    DeflatedFile,
    Dest,
    ExistingFile,
    ExtractedTarFile,
    FileFinder,
    File,
    GeneratedFile,
    HardlinkFile,
    JarFinder,
    TarFinder,
    ManifestFile,
    MercurialFile,
    MercurialRevisionFinder,
    MinifiedJavaScript,
    MinifiedProperties,
    PreprocessedFile,
)

# We don't have hglib installed everywhere.
try:
    import hglib
except ImportError:
    hglib = None

try:
    from mozpack.hg import MercurialNativeRevisionFinder
except ImportError:
    MercurialNativeRevisionFinder = None

from mozpack.mozjar import (
    JarReader,
    JarWriter,
)
from mozpack.chrome.manifest import (
    ManifestContent,
    ManifestResource,
    ManifestLocale,
    ManifestOverride,
)
import unittest
import mozfile
import mozunit
import os
import platform
import random
import six
import sys
import tarfile
import mozpack.path as mozpath
from tempfile import mkdtemp
from io import BytesIO


class TestWithTmpDir(unittest.TestCase):
    def setUp(self):
        self.tmpdir = mkdtemp()

        self.symlink_supported = False
        self.hardlink_supported = False

        # See comment in mozpack.files.AbsoluteSymlinkFile
        if hasattr(os, 'symlink') and platform.system() != 'Windows':
            dummy_path = self.tmppath('dummy_file')
            with open(dummy_path, 'a'):
                pass

            try:
                os.symlink(dummy_path, self.tmppath('dummy_symlink'))
                os.remove(self.tmppath('dummy_symlink'))
            except EnvironmentError:
                pass
            finally:
                os.remove(dummy_path)

            self.symlink_supported = True

        if hasattr(os, 'link'):
            dummy_path = self.tmppath('dummy_file')
            with open(dummy_path, 'a'):
                pass

            try:
                os.link(dummy_path, self.tmppath('dummy_hardlink'))
                os.remove(self.tmppath('dummy_hardlink'))
            except EnvironmentError:
                pass
            finally:
                os.remove(dummy_path)

            self.hardlink_supported = True

    def tearDown(self):
        mozfile.rmtree(self.tmpdir)

    def tmppath(self, relpath):
        return os.path.normpath(os.path.join(self.tmpdir, relpath))


class MockDest(BytesIO, Dest):
    def __init__(self):
        BytesIO.__init__(self)
        self.mode = None

    def read(self, length=-1):
        if self.mode != 'r':
            self.seek(0)
            self.mode = 'r'
        return BytesIO.read(self, length)

    def write(self, data):
        if self.mode != 'w':
            self.seek(0)
            self.truncate(0)
            self.mode = 'w'
        return BytesIO.write(self, data)

    def exists(self):
        return True

    def close(self):
        if self.mode:
            self.mode = None


class DestNoWrite(Dest):
    def write(self, data):
        raise RuntimeError


class TestDest(TestWithTmpDir):
    def test_dest(self):
        dest = Dest(self.tmppath('dest'))
        self.assertFalse(dest.exists())
        dest.write(b'foo')
        self.assertTrue(dest.exists())
        dest.write(b'foo')
        self.assertEqual(dest.read(4), b'foof')
        self.assertEqual(dest.read(), b'oo')
        self.assertEqual(dest.read(), b'')
        dest.write(b'bar')
        self.assertEqual(dest.read(4), b'bar')
        dest.close()
        self.assertEqual(dest.read(), b'bar')
        dest.write(b'foo')
        dest.close()
        dest.write(b'qux')
        self.assertEqual(dest.read(), b'qux')


rand = bytes(random.choice(b'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ')
             for i in six.moves.xrange(131597))
samples = [
    b'',
    b'test',
    b'fooo',
    b'same',
    b'same',
    b'Different and longer',
    rand,
    rand,
    rand[:-1] + b'_',
    b'test',
]


class TestFile(TestWithTmpDir):
    def test_file(self):
        '''
        Check that File.copy yields the proper content in the destination file
        in all situations that trigger different code paths:
        - different content
        - different content of the same size
        - same content
        - long content
        '''
        src = self.tmppath('src')
        dest = self.tmppath('dest')

        for content in samples:
            with open(src, 'wb') as tmp:
                tmp.write(content)
            # Ensure the destination file, when it exists, is older than the
            # source
            if os.path.exists(dest):
                time = os.path.getmtime(src) - 1
                os.utime(dest, (time, time))
            f = File(src)
            f.copy(dest)
            self.assertEqual(content, open(dest, 'rb').read())
            self.assertEqual(content, f.open().read())
            self.assertEqual(content, f.open().read())

    def test_file_dest(self):
        '''
        Similar to test_file, but for a destination object instead of
        a destination file. This ensures the destination object is being
        used properly by File.copy, ensuring that other subclasses of Dest
        will work.
        '''
        src = self.tmppath('src')
        dest = MockDest()

        for content in samples:
            with open(src, 'wb') as tmp:
                tmp.write(content)
            f = File(src)
            f.copy(dest)
            self.assertEqual(content, dest.getvalue())

    def test_file_open(self):
        '''
        Test whether File.open returns an appropriately reset file object.
        '''
        src = self.tmppath('src')
        content = b''.join(samples)
        with open(src, 'wb') as tmp:
            tmp.write(content)

        f = File(src)
        self.assertEqual(content[:42], f.open().read(42))
        self.assertEqual(content, f.open().read())

    def test_file_no_write(self):
        '''
        Test various conditions where File.copy is expected not to write
        in the destination file.
        '''
        src = self.tmppath('src')
        dest = self.tmppath('dest')

        with open(src, 'wb') as tmp:
            tmp.write(b'test')

        # Initial copy
        f = File(src)
        f.copy(dest)

        # Ensure subsequent copies won't trigger writes
        f.copy(DestNoWrite(dest))
        self.assertEqual(b'test', open(dest, 'rb').read())

        # When the source file is newer, but with the same content, no copy
        # should occur
        time = os.path.getmtime(src) - 1
        os.utime(dest, (time, time))
        f.copy(DestNoWrite(dest))
        self.assertEqual(b'test', open(dest, 'rb').read())

        # When the source file is older than the destination file, even with
        # different content, no copy should occur.
        with open(src, 'wb') as tmp:
            tmp.write(b'fooo')
        time = os.path.getmtime(dest) - 1
        os.utime(src, (time, time))
        f.copy(DestNoWrite(dest))
        self.assertEqual(b'test', open(dest, 'rb').read())

        # Double check that under conditions where a copy occurs, we would get
        # an exception.
        time = os.path.getmtime(src) - 1
        os.utime(dest, (time, time))
        self.assertRaises(RuntimeError, f.copy, DestNoWrite(dest))

        # skip_if_older=False is expected to force a copy in this situation.
        f.copy(dest, skip_if_older=False)
        self.assertEqual(b'fooo', open(dest, 'rb').read())


class TestAbsoluteSymlinkFile(TestWithTmpDir):
    def test_absolute_relative(self):
        AbsoluteSymlinkFile('/foo')

        with self.assertRaisesRegexp(ValueError, 'Symlink target not absolute'):
            AbsoluteSymlinkFile('./foo')

    def test_symlink_file(self):
        source = self.tmppath('test_path')
        with open(source, 'wt') as fh:
            fh.write('Hello world')

        s = AbsoluteSymlinkFile(source)
        dest = self.tmppath('symlink')
        self.assertTrue(s.copy(dest))

        if self.symlink_supported:
            self.assertTrue(os.path.islink(dest))
            link = os.readlink(dest)
            self.assertEqual(link, source)
        else:
            self.assertTrue(os.path.isfile(dest))
            content = open(dest).read()
            self.assertEqual(content, 'Hello world')

    def test_replace_file_with_symlink(self):
        # If symlinks are supported, an existing file should be replaced by a
        # symlink.
        source = self.tmppath('test_path')
        with open(source, 'wt') as fh:
            fh.write('source')

        dest = self.tmppath('dest')
        with open(dest, 'a'):
            pass

        s = AbsoluteSymlinkFile(source)
        s.copy(dest, skip_if_older=False)

        if self.symlink_supported:
            self.assertTrue(os.path.islink(dest))
            link = os.readlink(dest)
            self.assertEqual(link, source)
        else:
            self.assertTrue(os.path.isfile(dest))
            content = open(dest).read()
            self.assertEqual(content, 'source')

    def test_replace_symlink(self):
        if not self.symlink_supported:
            return

        source = self.tmppath('source')
        with open(source, 'a'):
            pass

        dest = self.tmppath('dest')

        os.symlink(self.tmppath('bad'), dest)
        self.assertTrue(os.path.islink(dest))

        s = AbsoluteSymlinkFile(source)
        self.assertTrue(s.copy(dest))

        self.assertTrue(os.path.islink(dest))
        link = os.readlink(dest)
        self.assertEqual(link, source)

    def test_noop(self):
        if not hasattr(os, 'symlink'):
            return

        source = self.tmppath('source')
        dest = self.tmppath('dest')

        with open(source, 'a'):
            pass

        os.symlink(source, dest)
        link = os.readlink(dest)
        self.assertEqual(link, source)

        s = AbsoluteSymlinkFile(source)
        self.assertFalse(s.copy(dest))

        link = os.readlink(dest)
        self.assertEqual(link, source)


class TestHardlinkFile(TestWithTmpDir):
    def test_absolute_relative(self):
        HardlinkFile('/foo')
        HardlinkFile('./foo')

    def test_hardlink_file(self):
        source = self.tmppath('test_path')
        with open(source, 'wt') as fh:
            fh.write('Hello world')

        s = HardlinkFile(source)
        dest = self.tmppath('hardlink')
        self.assertTrue(s.copy(dest))

        if self.hardlink_supported:
            source_stat = os.stat(source)
            dest_stat = os.stat(dest)
            self.assertEqual(source_stat.st_dev, dest_stat.st_dev)
            self.assertEqual(source_stat.st_ino, dest_stat.st_ino)
        else:
            self.assertTrue(os.path.isfile(dest))
            with open(dest) as f:
                content = f.read()
            self.assertEqual(content, 'Hello world')

    def test_replace_file_with_hardlink(self):
        # If hardlink are supported, an existing file should be replaced by a
        # symlink.
        source = self.tmppath('test_path')
        with open(source, 'wt') as fh:
            fh.write('source')

        dest = self.tmppath('dest')
        with open(dest, 'a'):
            pass

        s = HardlinkFile(source)
        s.copy(dest, skip_if_older=False)

        if self.hardlink_supported:
            source_stat = os.stat(source)
            dest_stat = os.stat(dest)
            self.assertEqual(source_stat.st_dev, dest_stat.st_dev)
            self.assertEqual(source_stat.st_ino, dest_stat.st_ino)
        else:
            self.assertTrue(os.path.isfile(dest))
            with open(dest) as f:
                content = f.read()
            self.assertEqual(content, 'source')

    def test_replace_hardlink(self):
        if not self.hardlink_supported:
            raise unittest.SkipTest('hardlink not supported')

        source = self.tmppath('source')
        with open(source, 'a'):
            pass

        dest = self.tmppath('dest')

        os.link(source, dest)

        s = HardlinkFile(source)
        self.assertFalse(s.copy(dest))

        source_stat = os.lstat(source)
        dest_stat = os.lstat(dest)
        self.assertEqual(source_stat.st_dev, dest_stat.st_dev)
        self.assertEqual(source_stat.st_ino, dest_stat.st_ino)

    def test_noop(self):
        if not self.hardlink_supported:
            raise unittest.SkipTest('hardlink not supported')

        source = self.tmppath('source')
        dest = self.tmppath('dest')

        with open(source, 'a'):
            pass

        os.link(source, dest)

        s = HardlinkFile(source)
        self.assertFalse(s.copy(dest))

        source_stat = os.lstat(source)
        dest_stat = os.lstat(dest)
        self.assertEqual(source_stat.st_dev, dest_stat.st_dev)
        self.assertEqual(source_stat.st_ino, dest_stat.st_ino)


class TestPreprocessedFile(TestWithTmpDir):
    def test_preprocess(self):
        '''
        Test that copying the file invokes the preprocessor
        '''
        src = self.tmppath('src')
        dest = self.tmppath('dest')

        with open(src, 'wb') as tmp:
            tmp.write(b'#ifdef FOO\ntest\n#endif')

        f = PreprocessedFile(src, depfile_path=None, marker='#', defines={'FOO': True})
        self.assertTrue(f.copy(dest))

        self.assertEqual(b'test\n', open(dest, 'rb').read())

    def test_preprocess_file_no_write(self):
        '''
        Test various conditions where PreprocessedFile.copy is expected not to
        write in the destination file.
        '''
        src = self.tmppath('src')
        dest = self.tmppath('dest')
        depfile = self.tmppath('depfile')

        with open(src, 'wb') as tmp:
            tmp.write(b'#ifdef FOO\ntest\n#endif')

        # Initial copy
        f = PreprocessedFile(src, depfile_path=depfile, marker='#', defines={'FOO': True})
        self.assertTrue(f.copy(dest))

        # Ensure subsequent copies won't trigger writes
        self.assertFalse(f.copy(DestNoWrite(dest)))
        self.assertEqual(b'test\n', open(dest, 'rb').read())

        # When the source file is older than the destination file, even with
        # different content, no copy should occur.
        with open(src, 'wb') as tmp:
            tmp.write(b'#ifdef FOO\nfooo\n#endif')
        time = os.path.getmtime(dest) - 1
        os.utime(src, (time, time))
        self.assertFalse(f.copy(DestNoWrite(dest)))
        self.assertEqual(b'test\n', open(dest, 'rb').read())

        # skip_if_older=False is expected to force a copy in this situation.
        self.assertTrue(f.copy(dest, skip_if_older=False))
        self.assertEqual(b'fooo\n', open(dest, 'rb').read())

    def test_preprocess_file_dependencies(self):
        '''
        Test that the preprocess runs if the dependencies of the source change
        '''
        src = self.tmppath('src')
        dest = self.tmppath('dest')
        incl = self.tmppath('incl')
        deps = self.tmppath('src.pp')

        with open(src, 'wb') as tmp:
            tmp.write(b'#ifdef FOO\ntest\n#endif')

        with open(incl, 'wb') as tmp:
            tmp.write(b'foo bar')

        # Initial copy
        f = PreprocessedFile(src, depfile_path=deps, marker='#', defines={'FOO': True})
        self.assertTrue(f.copy(dest))

        # Update the source so it #includes the include file.
        with open(src, 'wb') as tmp:
            tmp.write(b'#include incl\n')
        time = os.path.getmtime(dest) + 1
        os.utime(src, (time, time))
        self.assertTrue(f.copy(dest))
        self.assertEqual(b'foo bar', open(dest, 'rb').read())

        # If one of the dependencies changes, the file should be updated. The
        # mtime of the dependency is set after the destination file, to avoid
        # both files having the same time.
        with open(incl, 'wb') as tmp:
            tmp.write(b'quux')
        time = os.path.getmtime(dest) + 1
        os.utime(incl, (time, time))
        self.assertTrue(f.copy(dest))
        self.assertEqual(b'quux', open(dest, 'rb').read())

        # Perform one final copy to confirm that we don't run the preprocessor
        # again. We update the mtime of the destination so it's newer than the
        # input files. This would "just work" if we weren't changing
        time = os.path.getmtime(incl) + 1
        os.utime(dest, (time, time))
        self.assertFalse(f.copy(DestNoWrite(dest)))

    def test_replace_symlink(self):
        '''
        Test that if the destination exists, and is a symlink, the target of
        the symlink is not overwritten by the preprocessor output.
        '''
        if not self.symlink_supported:
            return

        source = self.tmppath('source')
        dest = self.tmppath('dest')
        pp_source = self.tmppath('pp_in')
        deps = self.tmppath('deps')

        with open(source, 'a'):
            pass

        os.symlink(source, dest)
        self.assertTrue(os.path.islink(dest))

        with open(pp_source, 'wb') as tmp:
            tmp.write(b'#define FOO\nPREPROCESSED')

        f = PreprocessedFile(pp_source, depfile_path=deps, marker='#',
                             defines={'FOO': True})
        self.assertTrue(f.copy(dest))

        self.assertEqual(b'PREPROCESSED', open(dest, 'rb').read())
        self.assertFalse(os.path.islink(dest))
        self.assertEqual(b'', open(source, 'rb').read())


class TestExistingFile(TestWithTmpDir):
    def test_required_missing_dest(self):
        with self.assertRaisesRegexp(ErrorMessage, 'Required existing file'):
            f = ExistingFile(required=True)
            f.copy(self.tmppath('dest'))

    def test_required_existing_dest(self):
        p = self.tmppath('dest')
        with open(p, 'a'):
            pass

        f = ExistingFile(required=True)
        f.copy(p)

    def test_optional_missing_dest(self):
        f = ExistingFile(required=False)
        f.copy(self.tmppath('dest'))

    def test_optional_existing_dest(self):
        p = self.tmppath('dest')
        with open(p, 'a'):
            pass

        f = ExistingFile(required=False)
        f.copy(p)


class TestGeneratedFile(TestWithTmpDir):
    def test_generated_file(self):
        '''
        Check that GeneratedFile.copy yields the proper content in the
        destination file in all situations that trigger different code paths
        (see TestFile.test_file)
        '''
        dest = self.tmppath('dest')

        for content in samples:
            f = GeneratedFile(content)
            f.copy(dest)
            self.assertEqual(content, open(dest, 'rb').read())

    def test_generated_file_open(self):
        '''
        Test whether GeneratedFile.open returns an appropriately reset file
        object.
        '''
        content = b''.join(samples)
        f = GeneratedFile(content)
        self.assertEqual(content[:42], f.open().read(42))
        self.assertEqual(content, f.open().read())

    def test_generated_file_no_write(self):
        '''
        Test various conditions where GeneratedFile.copy is expected not to
        write in the destination file.
        '''
        dest = self.tmppath('dest')

        # Initial copy
        f = GeneratedFile(b'test')
        f.copy(dest)

        # Ensure subsequent copies won't trigger writes
        f.copy(DestNoWrite(dest))
        self.assertEqual(b'test', open(dest, 'rb').read())

        # When using a new instance with the same content, no copy should occur
        f = GeneratedFile(b'test')
        f.copy(DestNoWrite(dest))
        self.assertEqual(b'test', open(dest, 'rb').read())

        # Double check that under conditions where a copy occurs, we would get
        # an exception.
        f = GeneratedFile(b'fooo')
        self.assertRaises(RuntimeError, f.copy, DestNoWrite(dest))

    def test_generated_file_function(self):
        '''
        Test GeneratedFile behavior with functions.
        '''
        dest = self.tmppath('dest')
        data = {
            'num_calls': 0,
        }

        def content():
            data['num_calls'] += 1
            return b'content'

        f = GeneratedFile(content)
        self.assertEqual(data['num_calls'], 0)
        f.copy(dest)
        self.assertEqual(data['num_calls'], 1)
        self.assertEqual(b'content', open(dest, 'rb').read())
        self.assertEqual(b'content', f.open().read())
        self.assertEqual(b'content', f.read())
        self.assertEqual(len(b'content'), f.size())
        self.assertEqual(data['num_calls'], 1)

        f.content = b'modified'
        f.copy(dest)
        self.assertEqual(data['num_calls'], 1)
        self.assertEqual(b'modified', open(dest, 'rb').read())
        self.assertEqual(b'modified', f.open().read())
        self.assertEqual(b'modified', f.read())
        self.assertEqual(len(b'modified'), f.size())

        f.content = content
        self.assertEqual(data['num_calls'], 1)
        self.assertEqual(b'content', f.read())
        self.assertEqual(data['num_calls'], 2)


class TestDeflatedFile(TestWithTmpDir):
    def test_deflated_file(self):
        '''
        Check that DeflatedFile.copy yields the proper content in the
        destination file in all situations that trigger different code paths
        (see TestFile.test_file)
        '''
        src = self.tmppath('src.jar')
        dest = self.tmppath('dest')

        contents = {}
        with JarWriter(src) as jar:
            for content in samples:
                name = ''.join(
                    random.choice(
                        'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ'
                    ) for i in range(8))
                jar.add(name, content, compress=True)
                contents[name] = content

        for j in JarReader(src):
            f = DeflatedFile(j)
            f.copy(dest)
            self.assertEqual(contents[j.filename], open(dest, 'rb').read())

    def test_deflated_file_open(self):
        '''
        Test whether DeflatedFile.open returns an appropriately reset file
        object.
        '''
        src = self.tmppath('src.jar')
        content = b''.join(samples)
        with JarWriter(src) as jar:
            jar.add('content', content)

        f = DeflatedFile(JarReader(src)['content'])
        self.assertEqual(content[:42], f.open().read(42))
        self.assertEqual(content, f.open().read())

    def test_deflated_file_no_write(self):
        '''
        Test various conditions where DeflatedFile.copy is expected not to
        write in the destination file.
        '''
        src = self.tmppath('src.jar')
        dest = self.tmppath('dest')

        with JarWriter(src) as jar:
            jar.add('test', b'test')
            jar.add('test2', b'test')
            jar.add('fooo', b'fooo')

        jar = JarReader(src)
        # Initial copy
        f = DeflatedFile(jar['test'])
        f.copy(dest)

        # Ensure subsequent copies won't trigger writes
        f.copy(DestNoWrite(dest))
        self.assertEqual(b'test', open(dest, 'rb').read())

        # When using a different file with the same content, no copy should
        # occur
        f = DeflatedFile(jar['test2'])
        f.copy(DestNoWrite(dest))
        self.assertEqual(b'test', open(dest, 'rb').read())

        # Double check that under conditions where a copy occurs, we would get
        # an exception.
        f = DeflatedFile(jar['fooo'])
        self.assertRaises(RuntimeError, f.copy, DestNoWrite(dest))


class TestManifestFile(TestWithTmpDir):
    def test_manifest_file(self):
        f = ManifestFile('chrome')
        f.add(ManifestContent('chrome', 'global', 'toolkit/content/global/'))
        f.add(ManifestResource('chrome', 'gre-resources', 'toolkit/res/'))
        f.add(ManifestResource('chrome/pdfjs', 'pdfjs', './'))
        f.add(ManifestContent('chrome/pdfjs', 'pdfjs', 'pdfjs'))
        f.add(ManifestLocale('chrome', 'browser', 'en-US',
                             'en-US/locale/browser/'))

        f.copy(self.tmppath('chrome.manifest'))
        self.assertEqual(open(self.tmppath('chrome.manifest')).readlines(), [
            'content global toolkit/content/global/\n',
            'resource gre-resources toolkit/res/\n',
            'resource pdfjs pdfjs/\n',
            'content pdfjs pdfjs/pdfjs\n',
            'locale browser en-US en-US/locale/browser/\n',
        ])

        self.assertRaises(
            ValueError,
            f.remove,
            ManifestContent('', 'global', 'toolkit/content/global/')
        )
        self.assertRaises(
            ValueError,
            f.remove,
            ManifestOverride('chrome', 'chrome://global/locale/netError.dtd',
                             'chrome://browser/locale/netError.dtd')
        )

        f.remove(ManifestContent('chrome', 'global',
                                 'toolkit/content/global/'))
        self.assertRaises(
            ValueError,
            f.remove,
            ManifestContent('chrome', 'global', 'toolkit/content/global/')
        )

        f.copy(self.tmppath('chrome.manifest'))
        content = open(self.tmppath('chrome.manifest'), 'rt').read()
        self.assertEqual(content[:42], f.open().read(42))
        self.assertEqual(content, f.open().read())


# Compiled typelib for the following IDL:
#     interface foo;
#     [scriptable, uuid(5f70da76-519c-4858-b71e-e3c92333e2d6)]
#     interface bar {
#         void bar(in foo f);
#     };
# We need to make this [scriptable] so it doesn't get deleted from the
# typelib.  We don't need to make the foo interfaces below [scriptable],
# because they will be automatically included by virtue of being an
# argument to a method of |bar|.
bar_xpt = GeneratedFile(
    b'\x58\x50\x43\x4F\x4D\x0A\x54\x79\x70\x65\x4C\x69\x62\x0D\x0A\x1A' +
    b'\x01\x02\x00\x02\x00\x00\x00\x7B\x00\x00\x00\x24\x00\x00\x00\x5C' +
    b'\x80\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00' +
    b'\x00\x00\x00\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00\x5F' +
    b'\x70\xDA\x76\x51\x9C\x48\x58\xB7\x1E\xE3\xC9\x23\x33\xE2\xD6\x00' +
    b'\x00\x00\x05\x00\x00\x00\x00\x00\x00\x00\x0D\x00\x66\x6F\x6F\x00' +
    b'\x62\x61\x72\x00\x62\x61\x72\x00\x00\x00\x00\x01\x00\x00\x00\x00' +
    b'\x09\x01\x80\x92\x00\x01\x80\x06\x00\x00\x80'
)

# Compiled typelib for the following IDL:
#     [uuid(3271bebc-927e-4bef-935e-44e0aaf3c1e5)]
#     interface foo {
#         void foo();
#     };
foo_xpt = GeneratedFile(
    b'\x58\x50\x43\x4F\x4D\x0A\x54\x79\x70\x65\x4C\x69\x62\x0D\x0A\x1A' +
    b'\x01\x02\x00\x01\x00\x00\x00\x57\x00\x00\x00\x24\x00\x00\x00\x40' +
    b'\x80\x00\x00\x32\x71\xBE\xBC\x92\x7E\x4B\xEF\x93\x5E\x44\xE0\xAA' +
    b'\xF3\xC1\xE5\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00\x09\x00' +
    b'\x66\x6F\x6F\x00\x66\x6F\x6F\x00\x00\x00\x00\x01\x00\x00\x00\x00' +
    b'\x05\x00\x80\x06\x00\x00\x00'
)

# Compiled typelib for the following IDL:
#     [uuid(7057f2aa-fdc2-4559-abde-08d939f7e80d)]
#     interface foo {
#         void foo();
#     };
foo2_xpt = GeneratedFile(
    b'\x58\x50\x43\x4F\x4D\x0A\x54\x79\x70\x65\x4C\x69\x62\x0D\x0A\x1A' +
    b'\x01\x02\x00\x01\x00\x00\x00\x57\x00\x00\x00\x24\x00\x00\x00\x40' +
    b'\x80\x00\x00\x70\x57\xF2\xAA\xFD\xC2\x45\x59\xAB\xDE\x08\xD9\x39' +
    b'\xF7\xE8\x0D\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00\x09\x00' +
    b'\x66\x6F\x6F\x00\x66\x6F\x6F\x00\x00\x00\x00\x01\x00\x00\x00\x00' +
    b'\x05\x00\x80\x06\x00\x00\x00'
)


class TestMinifiedProperties(TestWithTmpDir):
    def test_minified_properties(self):
        propLines = [
            '# Comments are removed',
            'foo = bar',
            '',
            '# Another comment',
        ]
        prop = GeneratedFile('\n'.join(propLines))
        self.assertEqual(MinifiedProperties(prop).open().readlines(),
                         ['foo = bar\n', '\n'])
        open(self.tmppath('prop'), 'w').write('\n'.join(propLines))
        MinifiedProperties(File(self.tmppath('prop'))) \
            .copy(self.tmppath('prop2'))
        self.assertEqual(open(self.tmppath('prop2')).readlines(),
                         ['foo = bar\n', '\n'])


class TestMinifiedJavaScript(TestWithTmpDir):
    orig_lines = [
        '// Comment line',
        'let foo = "bar";',
        'var bar = true;',
        '',
        '// Another comment',
    ]

    def test_minified_javascript(self):
        orig_f = GeneratedFile('\n'.join(self.orig_lines))
        min_f = MinifiedJavaScript(orig_f)

        mini_lines = min_f.open().readlines()
        self.assertTrue(mini_lines)
        self.assertTrue(len(mini_lines) < len(self.orig_lines))

    def _verify_command(self, code):
        our_dir = os.path.abspath(os.path.dirname(__file__))
        return [
            sys.executable,
            os.path.join(our_dir, 'support', 'minify_js_verify.py'),
            code,
        ]

    def test_minified_verify_success(self):
        orig_f = GeneratedFile('\n'.join(self.orig_lines))
        min_f = MinifiedJavaScript(orig_f,
                                   verify_command=self._verify_command('0'))

        mini_lines = min_f.open().readlines()
        self.assertTrue(mini_lines)
        self.assertTrue(len(mini_lines) < len(self.orig_lines))

    def test_minified_verify_failure(self):
        orig_f = GeneratedFile('\n'.join(self.orig_lines))
        errors.out = six.StringIO()
        min_f = MinifiedJavaScript(orig_f,
                                   verify_command=self._verify_command('1'))

        mini_lines = min_f.open().readlines()
        output = errors.out.getvalue()
        errors.out = sys.stderr
        self.assertEqual(output,
                         'Warning: JS minification verification failed for <unknown>:\n'
                         'Warning: Error message\n')
        self.assertEqual(mini_lines, orig_f.open().readlines())


class MatchTestTemplate(object):
    def prepare_match_test(self, with_dotfiles=False):
        self.add('bar')
        self.add('foo/bar')
        self.add('foo/baz')
        self.add('foo/qux/1')
        self.add('foo/qux/bar')
        self.add('foo/qux/2/test')
        self.add('foo/qux/2/test2')
        if with_dotfiles:
            self.add('foo/.foo')
            self.add('foo/.bar/foo')

    def do_match_test(self):
        self.do_check('', [
            'bar', 'foo/bar', 'foo/baz', 'foo/qux/1', 'foo/qux/bar',
            'foo/qux/2/test', 'foo/qux/2/test2'
        ])
        self.do_check('*', [
            'bar', 'foo/bar', 'foo/baz', 'foo/qux/1', 'foo/qux/bar',
            'foo/qux/2/test', 'foo/qux/2/test2'
        ])
        self.do_check('foo/qux', [
            'foo/qux/1', 'foo/qux/bar', 'foo/qux/2/test', 'foo/qux/2/test2'
        ])
        self.do_check('foo/b*', ['foo/bar', 'foo/baz'])
        self.do_check('baz', [])
        self.do_check('foo/foo', [])
        self.do_check('foo/*ar', ['foo/bar'])
        self.do_check('*ar', ['bar'])
        self.do_check('*/bar', ['foo/bar'])
        self.do_check('foo/*ux', [
            'foo/qux/1', 'foo/qux/bar', 'foo/qux/2/test', 'foo/qux/2/test2'
        ])
        self.do_check('foo/q*ux', [
            'foo/qux/1', 'foo/qux/bar', 'foo/qux/2/test', 'foo/qux/2/test2'
        ])
        self.do_check('foo/*/2/test*', ['foo/qux/2/test', 'foo/qux/2/test2'])
        self.do_check('**/bar', ['bar', 'foo/bar', 'foo/qux/bar'])
        self.do_check('foo/**/test', ['foo/qux/2/test'])
        self.do_check('foo', [
            'foo/bar', 'foo/baz', 'foo/qux/1', 'foo/qux/bar',
            'foo/qux/2/test', 'foo/qux/2/test2'
        ])
        self.do_check('foo/**', [
            'foo/bar', 'foo/baz', 'foo/qux/1', 'foo/qux/bar',
            'foo/qux/2/test', 'foo/qux/2/test2'
        ])
        self.do_check('**/2/test*', ['foo/qux/2/test', 'foo/qux/2/test2'])
        self.do_check('**/foo', [
            'foo/bar', 'foo/baz', 'foo/qux/1', 'foo/qux/bar',
            'foo/qux/2/test', 'foo/qux/2/test2'
        ])
        self.do_check('**/barbaz', [])
        self.do_check('f**/bar', ['foo/bar'])

    def do_finder_test(self, finder):
        self.assertTrue(finder.contains('foo/.foo'))
        self.assertTrue(finder.contains('foo/.bar'))
        self.assertTrue('foo/.foo' in [f for f, c in
                                       finder.find('foo/.foo')])
        self.assertTrue('foo/.bar/foo' in [f for f, c in
                                           finder.find('foo/.bar')])
        self.assertEqual(sorted([f for f, c in finder.find('foo/.*')]),
                         ['foo/.bar/foo', 'foo/.foo'])
        for pattern in ['foo', '**', '**/*', '**/foo', 'foo/*']:
            self.assertFalse('foo/.foo' in [f for f, c in
                                            finder.find(pattern)])
            self.assertFalse('foo/.bar/foo' in [f for f, c in
                                                finder.find(pattern)])
            self.assertEqual(sorted([f for f, c in finder.find(pattern)]),
                             sorted([f for f, c in finder
                                     if mozpath.match(f, pattern)]))


def do_check(test, finder, pattern, result):
    if result:
        test.assertTrue(finder.contains(pattern))
    else:
        test.assertFalse(finder.contains(pattern))
    test.assertEqual(sorted(list(f for f, c in finder.find(pattern))),
                     sorted(result))


class TestFileFinder(MatchTestTemplate, TestWithTmpDir):
    def add(self, path):
        ensureParentDir(self.tmppath(path))
        open(self.tmppath(path), 'wb').write(six.ensure_binary(path))

    def do_check(self, pattern, result):
        do_check(self, self.finder, pattern, result)

    def test_file_finder(self):
        self.prepare_match_test(with_dotfiles=True)
        self.finder = FileFinder(self.tmpdir)
        self.do_match_test()
        self.do_finder_test(self.finder)

    def test_get(self):
        self.prepare_match_test()
        finder = FileFinder(self.tmpdir)

        self.assertIsNone(finder.get('does-not-exist'))
        res = finder.get('bar')
        self.assertIsInstance(res, File)
        self.assertEqual(mozpath.normpath(res.path),
                         mozpath.join(self.tmpdir, 'bar'))

    def test_ignored_dirs(self):
        """Ignored directories should not have results returned."""
        self.prepare_match_test()
        self.add('fooz')

        # Present to ensure prefix matching doesn't exclude.
        self.add('foo/quxz')

        self.finder = FileFinder(self.tmpdir, ignore=['foo/qux'])

        self.do_check('**', ['bar', 'foo/bar', 'foo/baz', 'foo/quxz', 'fooz'])
        self.do_check('foo/*', ['foo/bar', 'foo/baz', 'foo/quxz'])
        self.do_check('foo/**', ['foo/bar', 'foo/baz', 'foo/quxz'])
        self.do_check('foo/qux/**', [])
        self.do_check('foo/qux/*', [])
        self.do_check('foo/qux/bar', [])
        self.do_check('foo/quxz', ['foo/quxz'])
        self.do_check('fooz', ['fooz'])

    def test_ignored_files(self):
        """Ignored files should not have results returned."""
        self.prepare_match_test()

        # Be sure prefix match doesn't get ignored.
        self.add('barz')

        self.finder = FileFinder(self.tmpdir, ignore=['foo/bar', 'bar'])
        self.do_check('**', ['barz', 'foo/baz', 'foo/qux/1', 'foo/qux/2/test',
                             'foo/qux/2/test2', 'foo/qux/bar'])
        self.do_check('foo/**', ['foo/baz', 'foo/qux/1', 'foo/qux/2/test',
                                 'foo/qux/2/test2', 'foo/qux/bar'])

    def test_ignored_patterns(self):
        """Ignore entries with patterns should be honored."""
        self.prepare_match_test()

        self.add('foo/quxz')

        self.finder = FileFinder(self.tmpdir, ignore=['foo/qux/*'])
        self.do_check('**', ['foo/bar', 'foo/baz', 'foo/quxz', 'bar'])
        self.do_check('foo/**', ['foo/bar', 'foo/baz', 'foo/quxz'])

    def test_dotfiles(self):
        """Finder can find files beginning with . is configured."""
        self.prepare_match_test(with_dotfiles=True)
        self.finder = FileFinder(self.tmpdir, find_dotfiles=True)
        self.do_check('**', ['bar', 'foo/.foo', 'foo/.bar/foo',
                             'foo/bar', 'foo/baz', 'foo/qux/1', 'foo/qux/bar',
                             'foo/qux/2/test', 'foo/qux/2/test2'])

    def test_dotfiles_plus_ignore(self):
        self.prepare_match_test(with_dotfiles=True)
        self.finder = FileFinder(self.tmpdir, find_dotfiles=True,
                                 ignore=['foo/.bar/**'])
        self.do_check('foo/**', ['foo/.foo', 'foo/bar', 'foo/baz',
                                 'foo/qux/1', 'foo/qux/bar', 'foo/qux/2/test', 'foo/qux/2/test2'])


class TestJarFinder(MatchTestTemplate, TestWithTmpDir):
    def add(self, path):
        self.jar.add(path, ensure_bytes(path), compress=True)

    def do_check(self, pattern, result):
        do_check(self, self.finder, pattern, result)

    def test_jar_finder(self):
        self.jar = JarWriter(file=self.tmppath('test.jar'))
        self.prepare_match_test()
        self.jar.finish()
        reader = JarReader(file=self.tmppath('test.jar'))
        self.finder = JarFinder(self.tmppath('test.jar'), reader)
        self.do_match_test()

        self.assertIsNone(self.finder.get('does-not-exist'))
        self.assertIsInstance(self.finder.get('bar'), DeflatedFile)


class TestTarFinder(MatchTestTemplate, TestWithTmpDir):
    def add(self, path):
        self.tar.addfile(tarfile.TarInfo(name=path))

    def do_check(self, pattern, result):
        do_check(self, self.finder, pattern, result)

    def test_tar_finder(self):
        self.tar = tarfile.open(name=self.tmppath('test.tar.bz2'),
                                mode='w:bz2')
        self.prepare_match_test()
        self.tar.close()
        with tarfile.open(name=self.tmppath('test.tar.bz2'),
                          mode='r:bz2') as tarreader:
            self.finder = TarFinder(self.tmppath('test.tar.bz2'), tarreader)
            self.do_match_test()

            self.assertIsNone(self.finder.get('does-not-exist'))
            self.assertIsInstance(self.finder.get('bar'), ExtractedTarFile)


class TestComposedFinder(MatchTestTemplate, TestWithTmpDir):
    def add(self, path, content=None):
        # Put foo/qux files under $tmp/b.
        if path.startswith('foo/qux/'):
            real_path = mozpath.join('b', path[8:])
        else:
            real_path = mozpath.join('a', path)
        ensureParentDir(self.tmppath(real_path))
        if not content:
            content = six.ensure_binary(path)
        open(self.tmppath(real_path), 'wb').write(content)

    def do_check(self, pattern, result):
        if '*' in pattern:
            return
        do_check(self, self.finder, pattern, result)

    def test_composed_finder(self):
        self.prepare_match_test()
        # Also add files in $tmp/a/foo/qux because ComposedFinder is
        # expected to mask foo/qux entirely with content from $tmp/b.
        ensureParentDir(self.tmppath('a/foo/qux/hoge'))
        open(self.tmppath('a/foo/qux/hoge'), 'wb').write(b'hoge')
        open(self.tmppath('a/foo/qux/bar'), 'wb').write(b'not the right content')
        self.finder = ComposedFinder({
            '': FileFinder(self.tmppath('a')),
            'foo/qux': FileFinder(self.tmppath('b')),
        })
        self.do_match_test()

        self.assertIsNone(self.finder.get('does-not-exist'))
        self.assertIsInstance(self.finder.get('bar'), File)


@unittest.skipUnless(hglib, 'hglib not available')
@unittest.skipIf(six.PY3 and os.name == 'nt',
                 'Does not currently work in Python3 on Windows')
class TestMercurialRevisionFinder(MatchTestTemplate, TestWithTmpDir):
    def setUp(self):
        super(TestMercurialRevisionFinder, self).setUp()
        hglib.init(self.tmpdir)
        self._clients = []

    def tearDown(self):
        # Ensure the hg client process is closed. Otherwise, Windows
        # may have trouble removing the repo directory because the process
        # has an open handle on it.
        for client in getattr(self, '_clients', []):
            if client.server:
                client.close()

        self._clients[:] = []

        super(TestMercurialRevisionFinder, self).tearDown()

    def _client(self):
        configs = (
            # b'' because py2 needs !unicode
            b'ui.username="Dummy User <dummy@example.com>"',
        )
        client = hglib.open(six.ensure_binary(self.tmpdir),
                            encoding=b'UTF-8',  # b'' because py2 needs !unicode
                            configs=configs)
        self._clients.append(client)
        return client

    def add(self, path):
        with self._client() as c:
            ensureParentDir(self.tmppath(path))
            with open(self.tmppath(path), 'wb') as fh:
                fh.write(six.ensure_binary(path))
            c.add(six.ensure_binary(self.tmppath(path)))

    def do_check(self, pattern, result):
        do_check(self, self.finder, pattern, result)

    def _get_finder(self, *args, **kwargs):
        f = MercurialRevisionFinder(*args, **kwargs)
        self._clients.append(f._client)
        return f

    def test_default_revision(self):
        self.prepare_match_test()
        with self._client() as c:
            c.commit('initial commit')

        self.finder = self._get_finder(self.tmpdir)
        self.do_match_test()

        self.assertIsNone(self.finder.get('does-not-exist'))
        self.assertIsInstance(self.finder.get('bar'), MercurialFile)

    def test_old_revision(self):
        with self._client() as c:
            with open(self.tmppath('foo'), 'wb') as fh:
                fh.write(b'foo initial')
            c.add(six.ensure_binary(self.tmppath('foo')))
            c.commit('initial')

            with open(self.tmppath('foo'), 'wb') as fh:
                fh.write(b'foo second')
            with open(self.tmppath('bar'), 'wb') as fh:
                fh.write(b'bar second')
            c.add(six.ensure_binary(self.tmppath('bar')))
            c.commit('second')
            # This wipes out the working directory, ensuring the finder isn't
            # finding anything from the filesystem.
            c.rawcommand([b'update', b'null'])

        finder = self._get_finder(self.tmpdir, '0')
        f = finder.get('foo')
        self.assertEqual(f.read(), b'foo initial')
        self.assertEqual(f.read(), b'foo initial',
                         'read again for good measure')
        self.assertIsNone(finder.get('bar'))

        finder = self._get_finder(self.tmpdir, rev='1')
        f = finder.get('foo')
        self.assertEqual(f.read(), b'foo second')
        f = finder.get('bar')
        self.assertEqual(f.read(), b'bar second')
        f = None

    def test_recognize_repo_paths(self):
        with self._client() as c:
            with open(self.tmppath('foo'), 'wb') as fh:
                fh.write(b'initial')
            c.add(six.ensure_binary(self.tmppath('foo')))
            c.commit('initial')
            c.rawcommand([b'update', b'null'])

        finder = self._get_finder(self.tmpdir, '0',
                                  recognize_repo_paths=True)
        with self.assertRaises(NotImplementedError):
            list(finder.find(''))

        with self.assertRaises(ValueError):
            finder.get('foo')
        with self.assertRaises(ValueError):
            finder.get('')

        f = finder.get(self.tmppath('foo'))
        self.assertIsInstance(f, MercurialFile)
        self.assertEqual(f.read(), b'initial')
        f = None


@unittest.skipUnless(MercurialNativeRevisionFinder, 'hgnative not available')
class TestMercurialNativeRevisionFinder(TestMercurialRevisionFinder):
    def _get_finder(self, *args, **kwargs):
        return MercurialNativeRevisionFinder(*args, **kwargs)


if __name__ == '__main__':
    mozunit.main()
