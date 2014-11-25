#!/usr/bin/env python

import unittest
import sys

sys.path.insert(0, '..')
import bitstring
from bitstring import MmapByteArray
from bitstring import Bits, BitArray, ConstByteStore, ByteStore

class Creation(unittest.TestCase):
    def testCreationFromBytes(self):
        s = Bits(bytes=b'\xa0\xff')
        self.assertEqual((s.len, s.hex), (16, 'a0ff'))
        s = Bits(bytes=b'abc', length=0)
        self.assertEqual(s, '')

    def testCreationFromBytesErrors(self):
        self.assertRaises(bitstring.CreationError, Bits, bytes=b'abc', length=25)

    def testCreationFromDataWithOffset(self):
        s1 = Bits(bytes=b'\x0b\x1c\x2f', offset=0, length=20)
        s2 = Bits(bytes=b'\xa0\xb1\xC2', offset=4)
        self.assertEqual((s2.len, s2.hex), (20, '0b1c2'))
        self.assertEqual((s1.len, s1.hex), (20, '0b1c2'))
        self.assertTrue(s1 == s2)

    def testCreationFromHex(self):
        s = Bits(hex='0xA0ff')
        self.assertEqual((s.len, s.hex), (16, 'a0ff'))
        s = Bits(hex='0x0x0X')
        self.assertEqual((s.length, s.hex), (0, ''))

    def testCreationFromHexWithWhitespace(self):
        s = Bits(hex='  \n0 X a  4e       \r3  \n')
        self.assertEqual(s.hex, 'a4e3')

    def testCreationFromHexErrors(self):
        self.assertRaises(bitstring.CreationError, Bits, hex='0xx0')
        self.assertRaises(bitstring.CreationError, Bits, hex='0xX0')
        self.assertRaises(bitstring.CreationError, Bits, hex='0Xx0')
        self.assertRaises(bitstring.CreationError, Bits, hex='-2e')
        # These really should fail, but it's awkward and not a big deal...
#        self.assertRaises(bitstring.CreationError, Bits, '0x2', length=2)
#        self.assertRaises(bitstring.CreationError, Bits, '0x3', offset=1)

    def testCreationFromBin(self):
        s = Bits(bin='1010000011111111')
        self.assertEqual((s.length, s.hex), (16, 'a0ff'))
        s = Bits(bin='00')[:1]
        self.assertEqual(s.bin, '0')
        s = Bits(bin=' 0000 \n 0001\r ')
        self.assertEqual(s.bin, '00000001')

    def testCreationFromBinWithWhitespace(self):
        s = Bits(bin='  \r\r\n0   B    00   1 1 \t0 ')
        self.assertEqual(s.bin, '00110')

    def testCreationFromOctErrors(self):
        s = Bits('0b00011')
        self.assertRaises(bitstring.InterpretError, s._getoct)
        self.assertRaises(bitstring.CreationError, s._setoct, '8')

    def testCreationFromUintWithOffset(self):
        self.assertRaises(bitstring.Error, Bits, uint=12, length=8, offset=1)

    def testCreationFromUintErrors(self):
        self.assertRaises(bitstring.CreationError, Bits, uint=-1, length=10)
        self.assertRaises(bitstring.CreationError, Bits, uint=12)
        self.assertRaises(bitstring.CreationError, Bits, uint=4, length=2)
        self.assertRaises(bitstring.CreationError, Bits, uint=0, length=0)
        self.assertRaises(bitstring.CreationError, Bits, uint=12, length=-12)

    def testCreationFromInt(self):
        s = Bits(int=0, length=4)
        self.assertEqual(s.bin, '0000')
        s = Bits(int=1, length=2)
        self.assertEqual(s.bin, '01')
        s = Bits(int=-1, length=11)
        self.assertEqual(s.bin, '11111111111')
        s = Bits(int=12, length=7)
        self.assertEqual(s.int, 12)
        s = Bits(int=-243, length=108)
        self.assertEqual((s.int, s.length), (-243, 108))
        for length in range(6, 10):
            for value in range(-17, 17):
                s = Bits(int=value, length=length)
                self.assertEqual((s.int, s.length), (value, length))
        s = Bits(int=10, length=8)

    def testCreationFromIntErrors(self):
        self.assertRaises(bitstring.CreationError, Bits, int=-1, length=0)
        self.assertRaises(bitstring.CreationError, Bits, int=12)
        self.assertRaises(bitstring.CreationError, Bits, int=4, length=3)
        self.assertRaises(bitstring.CreationError, Bits, int=-5, length=3)

    def testCreationFromSe(self):
        for i in range(-100, 10):
            s = Bits(se=i)
            self.assertEqual(s.se, i)

    def testCreationFromSeWithOffset(self):
        self.assertRaises(bitstring.CreationError, Bits, se=-13, offset=1)

    def testCreationFromSeErrors(self):
        self.assertRaises(bitstring.CreationError, Bits, se=-5, length=33)
        s = Bits(bin='001000')
        self.assertRaises(bitstring.InterpretError, s._getse)

    def testCreationFromUe(self):
        [self.assertEqual(Bits(ue=i).ue, i) for i in range(0, 20)]

    def testCreationFromUeWithOffset(self):
        self.assertRaises(bitstring.CreationError, Bits, ue=104, offset=2)

    def testCreationFromUeErrors(self):
        self.assertRaises(bitstring.CreationError, Bits, ue=-1)
        self.assertRaises(bitstring.CreationError, Bits, ue=1, length=12)
        s = Bits(bin='10')
        self.assertRaises(bitstring.InterpretError, s._getue)

    def testCreationFromBool(self):
        a = Bits('bool=1')
        self.assertEqual(a, 'bool=1')
        b = Bits('bool=0')
        self.assertEqual(b, [0])
        c = bitstring.pack('2*bool', 0, 1)
        self.assertEqual(c, '0b01')

    def testCreationKeywordError(self):
        self.assertRaises(bitstring.CreationError, Bits, squirrel=5)

    def testDataStoreType(self):
        a = Bits('0xf')
        self.assertEqual(type(a._datastore), bitstring.ConstByteStore)


class Initialisation(unittest.TestCase):
    def testEmptyInit(self):
        a = Bits()
        self.assertEqual(a, '')

    def testNoPos(self):
        a = Bits('0xabcdef')
        try:
            a.pos
        except AttributeError:
            pass
        else:
            assert False

    def testFind(self):
        a = Bits('0xabcd')
        r = a.find('0xbc')
        self.assertEqual(r[0], 4)
        r = a.find('0x23462346246', bytealigned=True)
        self.assertFalse(r)

    def testRfind(self):
        a = Bits('0b11101010010010')
        b = a.rfind('0b010')
        self.assertEqual(b[0], 11)

    def testFindAll(self):
        a = Bits('0b0010011')
        b = list(a.findall([1]))
        self.assertEqual(b, [2, 5, 6])


class Cut(unittest.TestCase):
    def testCut(self):
        s = Bits(30)
        for t in s.cut(3):
            self.assertEqual(t, [0] * 3)


class InterleavedExpGolomb(unittest.TestCase):
    def testCreation(self):
        s1 = Bits(uie=0)
        s2 = Bits(uie=1)
        self.assertEqual(s1, [1])
        self.assertEqual(s2, [0, 0, 1])
        s1 = Bits(sie=0)
        s2 = Bits(sie=-1)
        s3 = Bits(sie=1)
        self.assertEqual(s1, [1])
        self.assertEqual(s2, [0, 0, 1, 1])
        self.assertEqual(s3, [0, 0, 1, 0])

    def testCreationFromProperty(self):
        s = BitArray()
        s.uie = 45
        self.assertEqual(s.uie, 45)
        s.sie = -45
        self.assertEqual(s.sie, -45)

    def testInterpretation(self):
        for x in range(101):
            self.assertEqual(Bits(uie=x).uie, x)
        for x in range(-100, 100):
            self.assertEqual(Bits(sie=x).sie, x)

    def testErrors(self):
        for f in ['sie=100, 0b1001', '0b00', 'uie=100, 0b1001']:
            s = Bits(f)
            self.assertRaises(bitstring.InterpretError, s._getsie)
            self.assertRaises(bitstring.InterpretError, s._getuie)
        self.assertRaises(ValueError, Bits, 'uie=-10')


class FileBased(unittest.TestCase):
    def setUp(self):
        self.a = Bits(filename='smalltestfile')
        self.b = Bits(filename='smalltestfile', offset=16)
        self.c = Bits(filename='smalltestfile', offset=20, length=16)
        self.d = Bits(filename='smalltestfile', offset=20, length=4)

    def testCreationWithOffset(self):
        self.assertEqual(self.a, '0x0123456789abcdef')
        self.assertEqual(self.b, '0x456789abcdef')
        self.assertEqual(self.c, '0x5678')

    def testBitOperators(self):
        x = self.b[4:20]
        self.assertEqual(x, '0x5678')
        self.assertEqual((x & self.c).hex, self.c.hex)
        self.assertEqual(self.c ^ self.b[4:20], 16)
        self.assertEqual(self.a[23:36] | self.c[3:], self.c[3:])

    def testAddition(self):
        h = self.d + '0x1'
        x = self.a[20:24] + self.c[-4:] + self.c[8:12]
        self.assertEqual(x, '0x587')
        x = self.b + x
        self.assertEqual(x.hex, '456789abcdef587')
        x = BitArray(x)
        del x[12:24]
        self.assertEqual(x, '0x456abcdef587')
        
class Mmap(unittest.TestCase):
    def setUp(self):
        self.f = open('smalltestfile', 'rb')

    def tearDown(self):
        self.f.close()

    def testByteArrayEquivalence(self):
        a = MmapByteArray(self.f)
        self.assertEqual(a.bytelength, 8)
        self.assertEqual(len(a), 8)
        self.assertEqual(a[0], 0x01)
        self.assertEqual(a[1], 0x23)
        self.assertEqual(a[7], 0xef)
        self.assertEqual(a[0:1], bytearray([1]))
        self.assertEqual(a[:], bytearray([0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef]))
        self.assertEqual(a[2:4], bytearray([0x45, 0x67]))

    def testWithLength(self):
        a = MmapByteArray(self.f, 3)
        self.assertEqual(a[0], 0x01)
        self.assertEqual(len(a), 3)

    def testWithOffset(self):
        a = MmapByteArray(self.f, None, 5)
        self.assertEqual(len(a), 3)
        self.assertEqual(a[0], 0xab)

    def testWithLengthAndOffset(self):
        a = MmapByteArray(self.f, 3, 3)
        self.assertEqual(len(a), 3)
        self.assertEqual(a[0], 0x67)
        self.assertEqual(a[:], bytearray([0x67, 0x89, 0xab]))


class Comparisons(unittest.TestCase):
    def testUnorderable(self):
        a = Bits(5)
        b = Bits(5)
        self.assertRaises(TypeError, a.__lt__, b)
        self.assertRaises(TypeError, a.__gt__, b)
        self.assertRaises(TypeError, a.__le__, b)
        self.assertRaises(TypeError, a.__ge__, b)


class Subclassing(unittest.TestCase):

    def testIsInstance(self):
        class SubBits(bitstring.Bits): pass
        a = SubBits()
        self.assertTrue(isinstance(a, SubBits))

    def testClassType(self):
        class SubBits(bitstring.Bits): pass
        self.assertEqual(SubBits().__class__, SubBits)


class LongBoolConversion(unittest.TestCase):

    def testLongBool(self):
        a = Bits(1000)
        b = bool(a)
        self.assertTrue(b is False)


# Some basic tests for the private ByteStore classes

class ConstByteStoreCreation(unittest.TestCase):

    def testProperties(self):
        a = ConstByteStore(bytearray(b'abc'))
        self.assertEqual(a.bytelength, 3)
        self.assertEqual(a.offset, 0)
        self.assertEqual(a.bitlength, 24)
        self.assertEqual(a._rawarray, b'abc')

    def testGetBit(self):
        a = ConstByteStore(bytearray([0x0f]))
        self.assertEqual(a.getbit(0), False)
        self.assertEqual(a.getbit(3), False)
        self.assertEqual(a.getbit(4), True)
        self.assertEqual(a.getbit(7), True)

        b = ConstByteStore(bytearray([0x0f]), 7, 1)
        self.assertEqual(b.getbit(2), False)
        self.assertEqual(b.getbit(3), True)

    def testGetByte(self):
        a = ConstByteStore(bytearray(b'abcde'), 1, 13)
        self.assertEqual(a.getbyte(0), 97)
        self.assertEqual(a.getbyte(1), 98)
        self.assertEqual(a.getbyte(4), 101)


class PadToken(unittest.TestCase):

    def testCreation(self):
        a = Bits('pad:10')
        self.assertEqual(a, Bits(10))
        b = Bits('pad:0')
        self.assertEqual(b, Bits())
        c = Bits('0b11, pad:1, 0b111')
        self.assertEqual(c, Bits('0b110111'))

    def testPack(self):
        s = bitstring.pack('0b11, pad:3=5, 0b1')
        self.assertEqual(s.bin, '110001')
        d = bitstring.pack('pad:c', c=12)
        self.assertEqual(d, Bits(12))
        e = bitstring.pack('0xf, uint:12, pad:1, bin, pad:4, 0b10', 0, '111')
        self.assertEqual(e.bin, '11110000000000000111000010')

    def testUnpack(self):
        s = Bits('0b111000111')
        x, y = s.unpack('3, pad:3, 3')
        self.assertEqual((x, y), (7, 7))
        x, y = s.unpack('2, pad:2, bin')
        self.assertEqual((x, y), (3, '00111'))
        x = s.unpack('pad:1, pad:2, pad:3')
        self.assertEqual(x, [])


class ModifiedByAddingBug(unittest.TestCase):

    def testAdding(self):
        a = Bits('0b0')
        b = Bits('0b11')
        c = a + b
        self.assertEqual(c, '0b011')
        self.assertEqual(a, '0b0')
        self.assertEqual(b, '0b11')

    def testAdding2(self):
        a = Bits(100)
        b = Bits(101)
        c = a + b
        self.assertEqual(a, 100)
        self.assertEqual(b, 101)
        self.assertEqual(c, 201)
