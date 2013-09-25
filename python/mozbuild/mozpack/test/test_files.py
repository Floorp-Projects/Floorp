# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from mozbuild.util import ensureParentDir

from mozpack.errors import ErrorMessage
from mozpack.files import (
    AbsoluteSymlinkFile,
    DeflatedFile,
    Dest,
    ExistingFile,
    FileFinder,
    File,
    GeneratedFile,
    JarFinder,
    ManifestFile,
    MinifiedProperties,
    XPTFile,
)
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
import random
import string
import mozpack.path
from tempfile import mkdtemp
from io import BytesIO
from xpt import Typelib


class TestWithTmpDir(unittest.TestCase):
    def setUp(self):
        self.tmpdir = mkdtemp()

        self.symlink_supported = False

        if not hasattr(os, 'symlink'):
            return

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
        dest.write('foo')
        self.assertTrue(dest.exists())
        dest.write('foo')
        self.assertEqual(dest.read(4), 'foof')
        self.assertEqual(dest.read(), 'oo')
        self.assertEqual(dest.read(), '')
        dest.write('bar')
        self.assertEqual(dest.read(4), 'bar')
        dest.close()
        self.assertEqual(dest.read(), 'bar')
        dest.write('foo')
        dest.close()
        dest.write('qux')
        self.assertEqual(dest.read(), 'qux')

rand = ''.join(random.choice(string.letters) for i in xrange(131597))
samples = [
    '',
    'test',
    'fooo',
    'same',
    'same',
    'Different and longer',
    rand,
    rand,
    rand[:-1] + '_',
    'test'
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
        content = ''.join(samples)
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
            tmp.write('test')

        # Initial copy
        f = File(src)
        f.copy(dest)

        # Ensure subsequent copies won't trigger writes
        f.copy(DestNoWrite(dest))
        self.assertEqual('test', open(dest, 'rb').read())

        # When the source file is newer, but with the same content, no copy
        # should occur
        time = os.path.getmtime(src) - 1
        os.utime(dest, (time, time))
        f.copy(DestNoWrite(dest))
        self.assertEqual('test', open(dest, 'rb').read())

        # When the source file is older than the destination file, even with
        # different content, no copy should occur.
        with open(src, 'wb') as tmp:
            tmp.write('fooo')
        time = os.path.getmtime(dest) - 1
        os.utime(src, (time, time))
        f.copy(DestNoWrite(dest))
        self.assertEqual('test', open(dest, 'rb').read())

        # Double check that under conditions where a copy occurs, we would get
        # an exception.
        time = os.path.getmtime(src) - 1
        os.utime(dest, (time, time))
        self.assertRaises(RuntimeError, f.copy, DestNoWrite(dest))

        # skip_if_older=False is expected to force a copy in this situation.
        f.copy(dest, skip_if_older=False)
        self.assertEqual('fooo', open(dest, 'rb').read())


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
        content = ''.join(samples)
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
        f = GeneratedFile('test')
        f.copy(dest)

        # Ensure subsequent copies won't trigger writes
        f.copy(DestNoWrite(dest))
        self.assertEqual('test', open(dest, 'rb').read())

        # When using a new instance with the same content, no copy should occur
        f = GeneratedFile('test')
        f.copy(DestNoWrite(dest))
        self.assertEqual('test', open(dest, 'rb').read())

        # Double check that under conditions where a copy occurs, we would get
        # an exception.
        f = GeneratedFile('fooo')
        self.assertRaises(RuntimeError, f.copy, DestNoWrite(dest))


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
                name = ''.join(random.choice(string.letters)
                               for i in xrange(8))
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
        content = ''.join(samples)
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
            jar.add('test', 'test')
            jar.add('test2', 'test')
            jar.add('fooo', 'fooo')

        jar = JarReader(src)
        # Initial copy
        f = DeflatedFile(jar['test'])
        f.copy(dest)

        # Ensure subsequent copies won't trigger writes
        f.copy(DestNoWrite(dest))
        self.assertEqual('test', open(dest, 'rb').read())

        # When using a different file with the same content, no copy should
        # occur
        f = DeflatedFile(jar['test2'])
        f.copy(DestNoWrite(dest))
        self.assertEqual('test', open(dest, 'rb').read())

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
        content = open(self.tmppath('chrome.manifest')).read()
        self.assertEqual(content[:42], f.open().read(42))
        self.assertEqual(content, f.open().read())

# Compiled typelib for the following IDL:
#     interface foo;
#     [uuid(5f70da76-519c-4858-b71e-e3c92333e2d6)]
#     interface bar {
#         void bar(in foo f);
#     };
bar_xpt = GeneratedFile(
    b'\x58\x50\x43\x4F\x4D\x0A\x54\x79\x70\x65\x4C\x69\x62\x0D\x0A\x1A' +
    b'\x01\x02\x00\x02\x00\x00\x00\x7B\x00\x00\x00\x24\x00\x00\x00\x5C' +
    b'\x80\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00' +
    b'\x00\x00\x00\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00\x5F' +
    b'\x70\xDA\x76\x51\x9C\x48\x58\xB7\x1E\xE3\xC9\x23\x33\xE2\xD6\x00' +
    b'\x00\x00\x05\x00\x00\x00\x00\x00\x00\x00\x0D\x00\x66\x6F\x6F\x00' +
    b'\x62\x61\x72\x00\x62\x61\x72\x00\x00\x00\x00\x01\x00\x00\x00\x00' +
    b'\x09\x01\x80\x92\x00\x01\x80\x06\x00\x00\x00'
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


def read_interfaces(file):
    return dict((i.name, i) for i in Typelib.read(file).interfaces)


class TestXPTFile(TestWithTmpDir):
    def test_xpt_file(self):
        x = XPTFile()
        x.add(foo_xpt)
        x.add(bar_xpt)
        x.copy(self.tmppath('interfaces.xpt'))

        foo = read_interfaces(foo_xpt.open())
        foo2 = read_interfaces(foo2_xpt.open())
        bar = read_interfaces(bar_xpt.open())
        linked = read_interfaces(self.tmppath('interfaces.xpt'))
        self.assertEqual(foo['foo'], linked['foo'])
        self.assertEqual(bar['bar'], linked['bar'])

        x.remove(foo_xpt)
        x.copy(self.tmppath('interfaces2.xpt'))
        linked = read_interfaces(self.tmppath('interfaces2.xpt'))
        self.assertEqual(bar['foo'], linked['foo'])
        self.assertEqual(bar['bar'], linked['bar'])

        x.add(foo_xpt)
        x.copy(DestNoWrite(self.tmppath('interfaces.xpt')))
        linked = read_interfaces(self.tmppath('interfaces.xpt'))
        self.assertEqual(foo['foo'], linked['foo'])
        self.assertEqual(bar['bar'], linked['bar'])

        x = XPTFile()
        x.add(foo2_xpt)
        x.add(bar_xpt)
        x.copy(self.tmppath('interfaces.xpt'))
        linked = read_interfaces(self.tmppath('interfaces.xpt'))
        self.assertEqual(foo2['foo'], linked['foo'])
        self.assertEqual(bar['bar'], linked['bar'])

        x = XPTFile()
        x.add(foo_xpt)
        x.add(foo2_xpt)
        x.add(bar_xpt)
        from xpt import DataError
        self.assertRaises(DataError, x.copy, self.tmppath('interfaces.xpt'))


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
        open(self.tmppath('prop'), 'wb').write('\n'.join(propLines))
        MinifiedProperties(File(self.tmppath('prop'))) \
            .copy(self.tmppath('prop2'))
        self.assertEqual(open(self.tmppath('prop2')).readlines(),
                         ['foo = bar\n', '\n'])


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
                                     if mozpack.path.match(f, pattern)]))


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
        open(self.tmppath(path), 'wb').write(path)

    def do_check(self, pattern, result):
        do_check(self, self.finder, pattern, result)

    def test_file_finder(self):
        self.prepare_match_test(with_dotfiles=True)
        self.finder = FileFinder(self.tmpdir)
        self.do_match_test()
        self.do_finder_test(self.finder)


class TestJarFinder(MatchTestTemplate, TestWithTmpDir):
    def add(self, path):
        self.jar.add(path, path, compress=True)

    def do_check(self, pattern, result):
        do_check(self, self.finder, pattern, result)

    def test_jar_finder(self):
        self.jar = JarWriter(file=self.tmppath('test.jar'))
        self.prepare_match_test()
        self.jar.finish()
        reader = JarReader(file=self.tmppath('test.jar'))
        self.finder = JarFinder(self.tmppath('test.jar'), reader)
        self.do_match_test()


if __name__ == '__main__':
    mozunit.main()
