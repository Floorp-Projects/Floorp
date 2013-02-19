# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from mozpack.mozjar import (
    JarReaderError,
    JarWriterError,
    JarStruct,
    JarReader,
    JarWriter,
    Deflater,
    JarLog,
)
from collections import OrderedDict
from mozpack.test.test_files import MockDest
import unittest
import mozunit
from cStringIO import StringIO
from urllib import pathname2url
import mozpack.path
import os


class TestJarStruct(unittest.TestCase):
    class Foo(JarStruct):
        MAGIC = 0x01020304
        STRUCT = OrderedDict([
            ('foo', 'uint32'),
            ('bar', 'uint16'),
            ('qux', 'uint16'),
            ('length', 'uint16'),
            ('length2', 'uint16'),
            ('string', 'length'),
            ('string2', 'length2'),
        ])

    def test_jar_struct(self):
        foo = TestJarStruct.Foo()
        self.assertEqual(foo.signature, TestJarStruct.Foo.MAGIC)
        self.assertEqual(foo['foo'], 0)
        self.assertEqual(foo['bar'], 0)
        self.assertEqual(foo['qux'], 0)
        self.assertFalse('length' in foo)
        self.assertFalse('length2' in foo)
        self.assertEqual(foo['string'], '')
        self.assertEqual(foo['string2'], '')

        self.assertEqual(foo.size, 16)

        foo['foo'] = 0x42434445
        foo['bar'] = 0xabcd
        foo['qux'] = 0xef01
        foo['string'] = 'abcde'
        foo['string2'] = 'Arbitrarily long string'

        serialized = b'\x04\x03\x02\x01\x45\x44\x43\x42\xcd\xab\x01\xef' + \
                     b'\x05\x00\x17\x00abcdeArbitrarily long string'
        self.assertEqual(foo.size, len(serialized))
        foo_serialized = foo.serialize()
        self.assertEqual(foo_serialized, serialized)

    def do_test_read_jar_struct(self, data):
        self.assertRaises(JarReaderError, TestJarStruct.Foo, data)
        self.assertRaises(JarReaderError, TestJarStruct.Foo, data[2:])

        foo = TestJarStruct.Foo(data[1:])
        self.assertEqual(foo['foo'], 0x45444342)
        self.assertEqual(foo['bar'], 0xcdab)
        self.assertEqual(foo['qux'], 0x01ef)
        self.assertFalse('length' in foo)
        self.assertFalse('length2' in foo)
        self.assertEqual(foo['string'], '012345')
        self.assertEqual(foo['string2'], '67')

    def test_read_jar_struct(self):
        data = b'\x00\x04\x03\x02\x01\x42\x43\x44\x45\xab\xcd\xef' + \
               b'\x01\x06\x00\x02\x0001234567890'
        self.do_test_read_jar_struct(data)

    def test_read_jar_struct_memoryview(self):
        data = b'\x00\x04\x03\x02\x01\x42\x43\x44\x45\xab\xcd\xef' + \
               b'\x01\x06\x00\x02\x0001234567890'
        self.do_test_read_jar_struct(memoryview(data))


class TestDeflater(unittest.TestCase):
    def wrap(self, data):
        return data

    def test_deflater_no_compress(self):
        deflater = Deflater(False)
        deflater.write(self.wrap('abc'))
        self.assertFalse(deflater.compressed)
        self.assertEqual(deflater.uncompressed_size, 3)
        self.assertEqual(deflater.compressed_size, deflater.uncompressed_size)
        self.assertEqual(deflater.compressed_data, 'abc')
        self.assertEqual(deflater.crc32, 0x352441c2)

    def test_deflater_compress_no_gain(self):
        deflater = Deflater(True)
        deflater.write(self.wrap('abc'))
        self.assertFalse(deflater.compressed)
        self.assertEqual(deflater.uncompressed_size, 3)
        self.assertEqual(deflater.compressed_size, deflater.uncompressed_size)
        self.assertEqual(deflater.compressed_data, 'abc')
        self.assertEqual(deflater.crc32, 0x352441c2)

    def test_deflater_compress(self):
        deflater = Deflater(True)
        deflater.write(self.wrap('aaaaaaaaaaaaanopqrstuvwxyz'))
        self.assertTrue(deflater.compressed)
        self.assertEqual(deflater.uncompressed_size, 26)
        self.assertNotEqual(deflater.compressed_size,
                            deflater.uncompressed_size)
        self.assertEqual(deflater.crc32, 0xd46b97ed)
        # The CRC is the same as when not compressed
        deflater = Deflater(False)
        self.assertFalse(deflater.compressed)
        deflater.write(self.wrap('aaaaaaaaaaaaanopqrstuvwxyz'))
        self.assertEqual(deflater.crc32, 0xd46b97ed)


class TestDeflaterMemoryView(TestDeflater):
    def wrap(self, data):
        return memoryview(data)


class TestJar(unittest.TestCase):
    optimize = False

    def test_jar(self):
        s = MockDest()
        with JarWriter(fileobj=s, optimize=self.optimize) as jar:
            jar.add('foo', 'foo')
            self.assertRaises(JarWriterError, jar.add, 'foo', 'bar')
            jar.add('bar', 'aaaaaaaaaaaaanopqrstuvwxyz')
            jar.add('baz/qux', 'aaaaaaaaaaaaanopqrstuvwxyz', False)

        files = [j for j in JarReader(fileobj=s)]

        self.assertEqual(files[0].filename, 'foo')
        self.assertFalse(files[0].compressed)
        self.assertEqual(files[0].read(), 'foo')

        self.assertEqual(files[1].filename, 'bar')
        self.assertTrue(files[1].compressed)
        self.assertEqual(files[1].read(), 'aaaaaaaaaaaaanopqrstuvwxyz')

        self.assertEqual(files[2].filename, 'baz/qux')
        self.assertFalse(files[2].compressed)
        self.assertEqual(files[2].read(), 'aaaaaaaaaaaaanopqrstuvwxyz')

        s = MockDest()
        with JarWriter(fileobj=s, compress=False,
                       optimize=self.optimize) as jar:
            jar.add('bar', 'aaaaaaaaaaaaanopqrstuvwxyz')
            jar.add('foo', 'foo')
            jar.add('baz/qux', 'aaaaaaaaaaaaanopqrstuvwxyz', True)

        jar = JarReader(fileobj=s)
        files = [j for j in jar]

        self.assertEqual(files[0].filename, 'bar')
        self.assertFalse(files[0].compressed)
        self.assertEqual(files[0].read(), 'aaaaaaaaaaaaanopqrstuvwxyz')

        self.assertEqual(files[1].filename, 'foo')
        self.assertFalse(files[1].compressed)
        self.assertEqual(files[1].read(), 'foo')

        self.assertEqual(files[2].filename, 'baz/qux')
        self.assertTrue(files[2].compressed)
        self.assertEqual(files[2].read(), 'aaaaaaaaaaaaanopqrstuvwxyz')

        self.assertTrue('bar' in jar)
        self.assertTrue('foo' in jar)
        self.assertFalse('baz' in jar)
        self.assertTrue('baz/qux' in jar)
        self.assertTrue(jar['bar'], files[1])
        self.assertTrue(jar['foo'], files[0])
        self.assertTrue(jar['baz/qux'], files[2])

        s.seek(0)
        jar = JarReader(fileobj=s)
        self.assertTrue('bar' in jar)
        self.assertTrue('foo' in jar)
        self.assertFalse('baz' in jar)
        self.assertTrue('baz/qux' in jar)

        files[0].seek(0)
        self.assertEqual(jar['bar'].filename, files[0].filename)
        self.assertEqual(jar['bar'].compressed, files[0].compressed)
        self.assertEqual(jar['bar'].read(), files[0].read())

        files[1].seek(0)
        self.assertEqual(jar['foo'].filename, files[1].filename)
        self.assertEqual(jar['foo'].compressed, files[1].compressed)
        self.assertEqual(jar['foo'].read(), files[1].read())

        files[2].seek(0)
        self.assertEqual(jar['baz/qux'].filename, files[2].filename)
        self.assertEqual(jar['baz/qux'].compressed, files[2].compressed)
        self.assertEqual(jar['baz/qux'].read(), files[2].read())

    def test_rejar(self):
        s = MockDest()
        with JarWriter(fileobj=s, optimize=self.optimize) as jar:
            jar.add('foo', 'foo')
            jar.add('bar', 'aaaaaaaaaaaaanopqrstuvwxyz')
            jar.add('baz/qux', 'aaaaaaaaaaaaanopqrstuvwxyz', False)

        new = MockDest()
        with JarWriter(fileobj=new, optimize=self.optimize) as jar:
            for j in JarReader(fileobj=s):
                jar.add(j.filename, j)

        jar = JarReader(fileobj=new)
        files = [j for j in jar]

        self.assertEqual(files[0].filename, 'foo')
        self.assertFalse(files[0].compressed)
        self.assertEqual(files[0].read(), 'foo')

        self.assertEqual(files[1].filename, 'bar')
        self.assertTrue(files[1].compressed)
        self.assertEqual(files[1].read(), 'aaaaaaaaaaaaanopqrstuvwxyz')

        self.assertEqual(files[2].filename, 'baz/qux')
        self.assertTrue(files[2].compressed)
        self.assertEqual(files[2].read(), 'aaaaaaaaaaaaanopqrstuvwxyz')


class TestOptimizeJar(TestJar):
    optimize = True


class TestPreload(unittest.TestCase):
    def test_preload(self):
        s = MockDest()
        with JarWriter(fileobj=s) as jar:
            jar.add('foo', 'foo')
            jar.add('bar', 'abcdefghijklmnopqrstuvwxyz')
            jar.add('baz/qux', 'aaaaaaaaaaaaanopqrstuvwxyz')

        jar = JarReader(fileobj=s)
        self.assertEqual(jar.last_preloaded, None)

        with JarWriter(fileobj=s) as jar:
            jar.add('foo', 'foo')
            jar.add('bar', 'abcdefghijklmnopqrstuvwxyz')
            jar.add('baz/qux', 'aaaaaaaaaaaaanopqrstuvwxyz')
            jar.preload(['baz/qux', 'bar'])

        jar = JarReader(fileobj=s)
        self.assertEqual(jar.last_preloaded, 'bar')
        files = [j for j in jar]

        self.assertEqual(files[0].filename, 'baz/qux')
        self.assertEqual(files[1].filename, 'bar')
        self.assertEqual(files[2].filename, 'foo')


class TestJarLog(unittest.TestCase):
    def test_jarlog(self):
        base = 'file:' + pathname2url(os.path.abspath(os.curdir))
        s = StringIO('\n'.join([
            base + '/bar/baz.jar first',
            base + '/bar/baz.jar second',
            base + '/bar/baz.jar third',
            base + '/bar/baz.jar second',
            base + '/bar/baz.jar second',
            'jar:' + base + '/qux.zip!/omni.ja stuff',
            base + '/bar/baz.jar first',
            'jar:' + base + '/qux.zip!/omni.ja other/stuff',
            'jar:' + base + '/qux.zip!/omni.ja stuff',
            base + '/bar/baz.jar third',
            'jar:jar:' + base + '/qux.zip!/baz/baz.jar!/omni.ja nested/stuff',
            'jar:jar:jar:' + base + '/qux.zip!/baz/baz.jar!/foo.zip!/omni.ja' +
            ' deeply/nested/stuff',
        ]))
        log = JarLog(fileobj=s)
        canonicalize = lambda p: \
            mozpack.path.normsep(os.path.normcase(os.path.realpath(p)))
        baz_jar = canonicalize('bar/baz.jar')
        qux_zip = canonicalize('qux.zip')
        self.assertEqual(set(log.keys()), set([
            baz_jar,
            (qux_zip, 'omni.ja'),
            (qux_zip, 'baz/baz.jar', 'omni.ja'),
            (qux_zip, 'baz/baz.jar', 'foo.zip', 'omni.ja'),
        ]))
        self.assertEqual(log[baz_jar], [
            'first',
            'second',
            'third',
        ])
        self.assertEqual(log[(qux_zip, 'omni.ja')], [
            'stuff',
            'other/stuff',
        ])
        self.assertEqual(log[(qux_zip, 'baz/baz.jar', 'omni.ja')],
                         ['nested/stuff'])
        self.assertEqual(log[(qux_zip, 'baz/baz.jar', 'foo.zip',
                              'omni.ja')], ['deeply/nested/stuff'])

        # The above tests also indirectly check the value returned by
        # JarLog.canonicalize for various jar: and file: urls, but
        # JarLog.canonicalize also supports plain paths.
        self.assertEqual(JarLog.canonicalize(os.path.abspath('bar/baz.jar')),
                         baz_jar)
        self.assertEqual(JarLog.canonicalize('bar/baz.jar'), baz_jar)


if __name__ == '__main__':
    mozunit.main()
