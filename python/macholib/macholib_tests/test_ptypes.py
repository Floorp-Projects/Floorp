from macholib import ptypes

import unittest
import sys
if sys.version_info[:2] <= (2,6):
    import unittest2 as unittest
else:
    import unittest

try:
    from io import BytesIO
except ImportError:
    from cStringIO import StringIO as BytesIO
import mmap

try:
    long
except NameError:
    long = int


class TestPTypes (unittest.TestCase):
    if not hasattr(unittest.TestCase, 'assertIsSubclass'):
        def assertIsSubclass(self, class1, class2, message=None):
            self.assertTrue(issubclass(class1, class2),
                    message or "%r is not a subclass of %r"%(class1, class2))

    if not hasattr(unittest.TestCase, 'assertIsInstance'):
        def assertIsInstance(self, value, types, message=None):
            self.assertTrue(isinstance(value, types),
                    message or "%r is not an instance of %r"%(value, types))

    def test_sizeof(self):
        self.assertEqual(ptypes.sizeof(b"foobar"), 6)

        self.assertRaises(ValueError, ptypes.sizeof, [])
        self.assertRaises(ValueError, ptypes.sizeof, {})
        self.assertRaises(ValueError, ptypes.sizeof, b"foo".decode('ascii'))

        class M (object):
            pass

        m = M()
        m._size_ = 42
        self.assertEqual(ptypes.sizeof(m), 42)


    def verifyType(self, ptype, size, pytype, values):
        self.assertEqual(ptypes.sizeof(ptype), size)
        self.assertIsSubclass(ptype, pytype)

        for v in values:
            pv = ptype(v)
            packed = pv.to_str()
            self.assertIsInstance(packed, bytes)
            self.assertEqual(len(packed), size)

            unp = ptype.from_str(packed)
            self.assertIsInstance(unp, ptype)
            self.assertEqual(unp, pv)

            fp = BytesIO(packed)
            unp = ptype.from_fileobj(fp)
            fp.close()
            self.assertIsInstance(unp, ptype)
            self.assertEqual(unp, pv)

            fp = BytesIO()
            pv.to_fileobj(fp)
            data = fp.getvalue()
            fp.close()
            self.assertEqual(data, packed)

            mm = mmap.mmap(-1, size+20)
            mm[:] = b'\x00' * (size+20)
            pv.to_mmap(mm, 10)

            self.assertEqual(ptype.from_mmap(mm, 10), pv)
            self.assertEqual(mm[:], (b'\x00'*10) + packed + (b'\x00'*10))

            self.assertEqual(ptype.from_tuple((v,)), pv)

    def test_basic_types(self):
        self.verifyType(ptypes.p_char, 1, bytes, [b'a', b'b'])
        self.verifyType(ptypes.p_int8, 1, int, [1, 42, -4])
        self.verifyType(ptypes.p_uint8, 1, int, [1, 42, 253])

        self.verifyType(ptypes.p_int16, 2, int, [1, 400, -10, -5000])
        self.verifyType(ptypes.p_uint16, 2, int, [1, 400, 65000])

        self.verifyType(ptypes.p_int32, 4, int, [1, 400, 2**24, -10, -5000, -2**24])
        self.verifyType(ptypes.p_uint32, 4, long, [1, 400, 2*31+5, 65000])

        self.verifyType(ptypes.p_int64, 8, long, [1, 400, 2**43, -10, -5000, -2**43])
        self.verifyType(ptypes.p_uint64, 8, long, [1, 400, 2*63+5, 65000])

        self.verifyType(ptypes.p_float, 4, float, [1.0, 42.5])
        self.verifyType(ptypes.p_double, 8, float, [1.0, 42.5])

    def test_basic_types_deprecated(self):
        self.verifyType(ptypes.p_byte, 1, int, [1, 42, -4])
        self.verifyType(ptypes.p_ubyte, 1, int, [1, 42, 253])

        self.verifyType(ptypes.p_short, 2, int, [1, 400, -10, -5000])
        self.verifyType(ptypes.p_ushort, 2, int, [1, 400, 65000])

        self.verifyType(ptypes.p_int, 4, int, [1, 400, 2**24, -10, -5000, -2**24])
        self.verifyType(ptypes.p_uint, 4, long, [1, 400, 2*31+5, 65000])

        self.verifyType(ptypes.p_long, 4, int, [1, 400, 2**24, -10, -5000, -2**24])
        self.verifyType(ptypes.p_ulong, 4, long, [1, 400, 2*31+5, 65000])

        self.verifyType(ptypes.p_longlong, 8, long, [1, 400, 2**43, -10, -5000, -2**43])
        self.verifyType(ptypes.p_ulonglong, 8, long, [1, 400, 2*63+5, 65000])

class TestPTypesPrivate (unittest.TestCase):
    # These are tests for functions that aren't part of the public
    # API.

    def test_formatinfo(self):
        self.assertEqual(ptypes._formatinfo(">b"), (1, 1))
        self.assertEqual(ptypes._formatinfo(">h"), (2, 1))
        self.assertEqual(ptypes._formatinfo(">HhL"), (8, 3))
        self.assertEqual(ptypes._formatinfo("<b"), (1, 1))
        self.assertEqual(ptypes._formatinfo("<h"), (2, 1))
        self.assertEqual(ptypes._formatinfo("<HhL"), (8, 3))


class MyStructure (ptypes.Structure):
    _fields_ = (
        ('foo', ptypes.p_int32),
        ('bar', ptypes.p_uint8),
    )

class MyFunStructure (ptypes.Structure):
    _fields_ = (
        ('fun', ptypes.p_char),
        ('mystruct', MyStructure),
    )

class TestPTypesSimple (unittest.TestCase):
    # Quick port of tests that used to be part of
    # the macholib.ptypes source code
    #
    # Moving these in a structured manner to TestPTypes
    # would be nice, but is not extremely important.

    def testBasic(self):
        for endian in '><':
            kw = dict(_endian_=endian)
            MYSTRUCTURE = b'\x00\x11\x22\x33\xFF'
            for fn, args in [
                        ('from_str', (MYSTRUCTURE,)),
                        ('from_mmap', (MYSTRUCTURE, 0)),
                        ('from_fileobj', (BytesIO(MYSTRUCTURE),)),
                    ]:
                myStructure = getattr(MyStructure, fn)(*args, **kw)
                if endian == '>':
                    self.assertEqual(myStructure.foo, 0x00112233)
                else:
                    self.assertEqual( myStructure.foo, 0x33221100)
                self.assertEqual(myStructure.bar, 0xFF)
                self.assertEqual(myStructure.to_str(), MYSTRUCTURE)

            MYFUNSTRUCTURE = b'!' + MYSTRUCTURE
            for fn, args in [
                        ('from_str', (MYFUNSTRUCTURE,)),
                        ('from_mmap', (MYFUNSTRUCTURE, 0)),
                        ('from_fileobj', (BytesIO(MYFUNSTRUCTURE),)),
                    ]:
                myFunStructure = getattr(MyFunStructure, fn)(*args, **kw)
                self.assertEqual(myFunStructure.mystruct, myStructure)
                self.assertEqual(myFunStructure.fun, b'!', (myFunStructure.fun, b'!'))
                self.assertEqual(myFunStructure.to_str(), MYFUNSTRUCTURE)

            sio = BytesIO()
            myFunStructure.to_fileobj(sio)
            self.assertEqual(sio.getvalue(), MYFUNSTRUCTURE)

            mm = mmap.mmap(-1, ptypes.sizeof(MyFunStructure) * 2)
            mm[:] = b'\x00' * (ptypes.sizeof(MyFunStructure) * 2)
            myFunStructure.to_mmap(mm, 0)
            self.assertEqual(MyFunStructure.from_mmap(mm, 0, **kw), myFunStructure)
            self.assertEqual(mm[:ptypes.sizeof(MyFunStructure)], MYFUNSTRUCTURE)
            self.assertEqual(mm[ptypes.sizeof(MyFunStructure):], b'\x00' * ptypes.sizeof(MyFunStructure))
            myFunStructure.to_mmap(mm, ptypes.sizeof(MyFunStructure))
            self.assertEqual(mm[:], MYFUNSTRUCTURE + MYFUNSTRUCTURE)
            self.assertEqual(MyFunStructure.from_mmap(mm, ptypes.sizeof(MyFunStructure), **kw), myFunStructure)

if __name__ == "__main__":
    unittest.main()
