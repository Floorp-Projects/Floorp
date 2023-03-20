# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import unittest
from collections import OrderedDict

import mozunit
import six

import mozpack.path as mozpath
from mozpack.files import FileFinder
from mozpack.mozjar import (
    Deflater,
    JarLog,
    JarReader,
    JarReaderError,
    JarStruct,
    JarWriter,
    JarWriterError,
)
from mozpack.test.test_files import MockDest

test_data_path = mozpath.abspath(mozpath.dirname(__file__))
test_data_path = mozpath.join(test_data_path, "data")


class TestJarStruct(unittest.TestCase):
    class Foo(JarStruct):
        MAGIC = 0x01020304
        STRUCT = OrderedDict(
            [
                ("foo", "uint32"),
                ("bar", "uint16"),
                ("qux", "uint16"),
                ("length", "uint16"),
                ("length2", "uint16"),
                ("string", "length"),
                ("string2", "length2"),
            ]
        )

    def test_jar_struct(self):
        foo = TestJarStruct.Foo()
        self.assertEqual(foo.signature, TestJarStruct.Foo.MAGIC)
        self.assertEqual(foo["foo"], 0)
        self.assertEqual(foo["bar"], 0)
        self.assertEqual(foo["qux"], 0)
        self.assertFalse("length" in foo)
        self.assertFalse("length2" in foo)
        self.assertEqual(foo["string"], "")
        self.assertEqual(foo["string2"], "")

        self.assertEqual(foo.size, 16)

        foo["foo"] = 0x42434445
        foo["bar"] = 0xABCD
        foo["qux"] = 0xEF01
        foo["string"] = "abcde"
        foo["string2"] = "Arbitrarily long string"

        serialized = (
            b"\x04\x03\x02\x01\x45\x44\x43\x42\xcd\xab\x01\xef"
            + b"\x05\x00\x17\x00abcdeArbitrarily long string"
        )
        self.assertEqual(foo.size, len(serialized))
        foo_serialized = foo.serialize()
        self.assertEqual(foo_serialized, serialized)

    def do_test_read_jar_struct(self, data):
        self.assertRaises(JarReaderError, TestJarStruct.Foo, data)
        self.assertRaises(JarReaderError, TestJarStruct.Foo, data[2:])

        foo = TestJarStruct.Foo(data[1:])
        self.assertEqual(foo["foo"], 0x45444342)
        self.assertEqual(foo["bar"], 0xCDAB)
        self.assertEqual(foo["qux"], 0x01EF)
        self.assertFalse("length" in foo)
        self.assertFalse("length2" in foo)
        self.assertEqual(foo["string"], b"012345")
        self.assertEqual(foo["string2"], b"67")

    def test_read_jar_struct(self):
        data = (
            b"\x00\x04\x03\x02\x01\x42\x43\x44\x45\xab\xcd\xef"
            + b"\x01\x06\x00\x02\x0001234567890"
        )
        self.do_test_read_jar_struct(data)

    def test_read_jar_struct_memoryview(self):
        data = (
            b"\x00\x04\x03\x02\x01\x42\x43\x44\x45\xab\xcd\xef"
            + b"\x01\x06\x00\x02\x0001234567890"
        )
        self.do_test_read_jar_struct(memoryview(data))


class TestDeflater(unittest.TestCase):
    def wrap(self, data):
        return data

    def test_deflater_no_compress(self):
        deflater = Deflater(False)
        deflater.write(self.wrap(b"abc"))
        self.assertFalse(deflater.compressed)
        self.assertEqual(deflater.uncompressed_size, 3)
        self.assertEqual(deflater.compressed_size, deflater.uncompressed_size)
        self.assertEqual(deflater.compressed_data, b"abc")
        self.assertEqual(deflater.crc32, 0x352441C2)

    def test_deflater_compress_no_gain(self):
        deflater = Deflater(True)
        deflater.write(self.wrap(b"abc"))
        self.assertFalse(deflater.compressed)
        self.assertEqual(deflater.uncompressed_size, 3)
        self.assertEqual(deflater.compressed_size, deflater.uncompressed_size)
        self.assertEqual(deflater.compressed_data, b"abc")
        self.assertEqual(deflater.crc32, 0x352441C2)

    def test_deflater_compress(self):
        deflater = Deflater(True)
        deflater.write(self.wrap(b"aaaaaaaaaaaaanopqrstuvwxyz"))
        self.assertTrue(deflater.compressed)
        self.assertEqual(deflater.uncompressed_size, 26)
        self.assertNotEqual(deflater.compressed_size, deflater.uncompressed_size)
        self.assertEqual(deflater.crc32, 0xD46B97ED)
        # The CRC is the same as when not compressed
        deflater = Deflater(False)
        self.assertFalse(deflater.compressed)
        deflater.write(self.wrap(b"aaaaaaaaaaaaanopqrstuvwxyz"))
        self.assertEqual(deflater.crc32, 0xD46B97ED)

    def test_deflater_empty(self):
        deflater = Deflater(False)
        self.assertFalse(deflater.compressed)
        self.assertEqual(deflater.uncompressed_size, 0)
        self.assertEqual(deflater.compressed_size, deflater.uncompressed_size)
        self.assertEqual(deflater.compressed_data, b"")
        self.assertEqual(deflater.crc32, 0)


class TestDeflaterMemoryView(TestDeflater):
    def wrap(self, data):
        return memoryview(data)


class TestJar(unittest.TestCase):
    def test_jar(self):
        s = MockDest()
        with JarWriter(fileobj=s) as jar:
            jar.add("foo", b"foo")
            self.assertRaises(JarWriterError, jar.add, "foo", b"bar")
            jar.add("bar", b"aaaaaaaaaaaaanopqrstuvwxyz")
            jar.add("baz/qux", b"aaaaaaaaaaaaanopqrstuvwxyz", False)
            jar.add("baz\\backslash", b"aaaaaaaaaaaaaaa")

        files = [j for j in JarReader(fileobj=s)]

        self.assertEqual(files[0].filename, "foo")
        self.assertFalse(files[0].compressed)
        self.assertEqual(files[0].read(), b"foo")

        self.assertEqual(files[1].filename, "bar")
        self.assertTrue(files[1].compressed)
        self.assertEqual(files[1].read(), b"aaaaaaaaaaaaanopqrstuvwxyz")

        self.assertEqual(files[2].filename, "baz/qux")
        self.assertFalse(files[2].compressed)
        self.assertEqual(files[2].read(), b"aaaaaaaaaaaaanopqrstuvwxyz")

        if os.sep == "\\":
            self.assertEqual(
                files[3].filename,
                "baz/backslash",
                "backslashes in filenames on Windows should get normalized",
            )
        else:
            self.assertEqual(
                files[3].filename,
                "baz\\backslash",
                "backslashes in filenames on POSIX platform are untouched",
            )

        s = MockDest()
        with JarWriter(fileobj=s, compress=False) as jar:
            jar.add("bar", b"aaaaaaaaaaaaanopqrstuvwxyz")
            jar.add("foo", b"foo")
            jar.add("baz/qux", b"aaaaaaaaaaaaanopqrstuvwxyz", True)

        jar = JarReader(fileobj=s)
        files = [j for j in jar]

        self.assertEqual(files[0].filename, "bar")
        self.assertFalse(files[0].compressed)
        self.assertEqual(files[0].read(), b"aaaaaaaaaaaaanopqrstuvwxyz")

        self.assertEqual(files[1].filename, "foo")
        self.assertFalse(files[1].compressed)
        self.assertEqual(files[1].read(), b"foo")

        self.assertEqual(files[2].filename, "baz/qux")
        self.assertTrue(files[2].compressed)
        self.assertEqual(files[2].read(), b"aaaaaaaaaaaaanopqrstuvwxyz")

        self.assertTrue("bar" in jar)
        self.assertTrue("foo" in jar)
        self.assertFalse("baz" in jar)
        self.assertTrue("baz/qux" in jar)
        self.assertTrue(jar["bar"], files[1])
        self.assertTrue(jar["foo"], files[0])
        self.assertTrue(jar["baz/qux"], files[2])

        s.seek(0)
        jar = JarReader(fileobj=s)
        self.assertTrue("bar" in jar)
        self.assertTrue("foo" in jar)
        self.assertFalse("baz" in jar)
        self.assertTrue("baz/qux" in jar)

        files[0].seek(0)
        self.assertEqual(jar["bar"].filename, files[0].filename)
        self.assertEqual(jar["bar"].compressed, files[0].compressed)
        self.assertEqual(jar["bar"].read(), files[0].read())

        files[1].seek(0)
        self.assertEqual(jar["foo"].filename, files[1].filename)
        self.assertEqual(jar["foo"].compressed, files[1].compressed)
        self.assertEqual(jar["foo"].read(), files[1].read())

        files[2].seek(0)
        self.assertEqual(jar["baz/qux"].filename, files[2].filename)
        self.assertEqual(jar["baz/qux"].compressed, files[2].compressed)
        self.assertEqual(jar["baz/qux"].read(), files[2].read())

    def test_rejar(self):
        s = MockDest()
        with JarWriter(fileobj=s) as jar:
            jar.add("foo", b"foo")
            jar.add("bar", b"aaaaaaaaaaaaanopqrstuvwxyz")
            jar.add("baz/qux", b"aaaaaaaaaaaaanopqrstuvwxyz", False)

        new = MockDest()
        with JarWriter(fileobj=new) as jar:
            for j in JarReader(fileobj=s):
                jar.add(j.filename, j)

        jar = JarReader(fileobj=new)
        files = [j for j in jar]

        self.assertEqual(files[0].filename, "foo")
        self.assertFalse(files[0].compressed)
        self.assertEqual(files[0].read(), b"foo")

        self.assertEqual(files[1].filename, "bar")
        self.assertTrue(files[1].compressed)
        self.assertEqual(files[1].read(), b"aaaaaaaaaaaaanopqrstuvwxyz")

        self.assertEqual(files[2].filename, "baz/qux")
        self.assertTrue(files[2].compressed)
        self.assertEqual(files[2].read(), b"aaaaaaaaaaaaanopqrstuvwxyz")

    def test_add_from_finder(self):
        s = MockDest()
        with JarWriter(fileobj=s) as jar:
            finder = FileFinder(test_data_path)
            for p, f in finder.find("test_data"):
                jar.add("test_data", f)

        jar = JarReader(fileobj=s)
        files = [j for j in jar]

        self.assertEqual(files[0].filename, "test_data")
        self.assertFalse(files[0].compressed)
        self.assertEqual(files[0].read(), b"test_data")


class TestPreload(unittest.TestCase):
    def test_preload(self):
        s = MockDest()
        with JarWriter(fileobj=s) as jar:
            jar.add("foo", b"foo")
            jar.add("bar", b"abcdefghijklmnopqrstuvwxyz")
            jar.add("baz/qux", b"aaaaaaaaaaaaanopqrstuvwxyz")

        jar = JarReader(fileobj=s)
        self.assertEqual(jar.last_preloaded, None)

        with JarWriter(fileobj=s) as jar:
            jar.add("foo", b"foo")
            jar.add("bar", b"abcdefghijklmnopqrstuvwxyz")
            jar.add("baz/qux", b"aaaaaaaaaaaaanopqrstuvwxyz")
            jar.preload(["baz/qux", "bar"])

        jar = JarReader(fileobj=s)
        self.assertEqual(jar.last_preloaded, "bar")
        files = [j for j in jar]

        self.assertEqual(files[0].filename, "baz/qux")
        self.assertEqual(files[1].filename, "bar")
        self.assertEqual(files[2].filename, "foo")


class TestJarLog(unittest.TestCase):
    def test_jarlog(self):
        s = six.moves.cStringIO(
            "\n".join(
                [
                    "bar/baz.jar first",
                    "bar/baz.jar second",
                    "bar/baz.jar third",
                    "bar/baz.jar second",
                    "bar/baz.jar second",
                    "omni.ja stuff",
                    "bar/baz.jar first",
                    "omni.ja other/stuff",
                    "omni.ja stuff",
                    "bar/baz.jar third",
                ]
            )
        )
        log = JarLog(fileobj=s)
        self.assertEqual(
            set(log.keys()),
            set(
                [
                    "bar/baz.jar",
                    "omni.ja",
                ]
            ),
        )
        self.assertEqual(
            log["bar/baz.jar"],
            [
                "first",
                "second",
                "third",
            ],
        )
        self.assertEqual(
            log["omni.ja"],
            [
                "stuff",
                "other/stuff",
            ],
        )


if __name__ == "__main__":
    mozunit.main()
