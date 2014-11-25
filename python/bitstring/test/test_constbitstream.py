#!/usr/bin/env python

import unittest
import sys
sys.path.insert(0, '..')
import bitstring
from bitstring import ConstBitStream as CBS

class All(unittest.TestCase):
    def testFromFile(self):
        s = CBS(filename='test.m1v')
        self.assertEqual(s[0:32].hex, '000001b3')
        self.assertEqual(s.read(8 * 4).hex, '000001b3')
        width = s.read(12).uint
        height = s.read(12).uint
        self.assertEqual((width, height), (352, 288))


class InterleavedExpGolomb(unittest.TestCase):
    def testReading(self):
        s = CBS(uie=333)
        a = s.read('uie')
        self.assertEqual(a, 333)
        s = CBS('uie=12, sie=-9, sie=9, uie=1000000')
        u = s.unpack('uie, 2*sie, uie')
        self.assertEqual(u, [12, -9, 9, 1000000])

    def testReadingErrors(self):
        s = CBS(10)
        self.assertRaises(bitstring.ReadError, s.read, 'uie')
        self.assertEqual(s.pos, 0)
        self.assertRaises(bitstring.ReadError, s.read, 'sie')
        self.assertEqual(s.pos, 0)


class ReadTo(unittest.TestCase):
    def testByteAligned(self):
        a = CBS('0xaabb00aa00bb')
        b = a.readto('0x00', bytealigned=True)
        self.assertEqual(b, '0xaabb00')
        self.assertEqual(a.bytepos, 3)
        b = a.readto('0xaa', bytealigned=True)
        self.assertEqual(b, '0xaa')
        self.assertRaises(bitstring.ReadError, a.readto, '0xcc', bytealigned=True)

    def testNotAligned(self):
        a = CBS('0b00111001001010011011')
        a.pos = 1
        self.assertEqual(a.readto('0b00'), '0b011100')
        self.assertEqual(a.readto('0b110'), '0b10010100110')
        self.assertRaises(ValueError, a.readto, '')

    def testDisallowIntegers(self):
        a = CBS('0x0f')
        self.assertRaises(ValueError, a.readto, 4)

    def testReadingLines(self):
        s = b"This is a test\nof reading lines\nof text\n"
        b = CBS(bytes=s)
        n = bitstring.Bits(bytes=b'\n')
        self.assertEqual(b.readto(n).bytes, b'This is a test\n')
        self.assertEqual(b.readto(n).bytes, b'of reading lines\n')
        self.assertEqual(b.readto(n).bytes, b'of text\n')


class Subclassing(unittest.TestCase):

    def testIsInstance(self):
        class SubBits(CBS): pass
        a = SubBits()
        self.assertTrue(isinstance(a, SubBits))

    def testClassType(self):
        class SubBits(CBS): pass
        self.assertEqual(SubBits().__class__, SubBits)


class PadToken(unittest.TestCase):

    def testRead(self):
        s = CBS('0b100011110001')
        a = s.read('pad:1')
        self.assertEqual(a, None)
        self.assertEqual(s.pos, 1)
        a = s.read(3)
        self.assertEqual(a, CBS('0b000'))
        a = s.read('pad:0')
        self.assertEqual(a, None)
        self.assertEqual(s.pos, 4)

    def testReadList(self):
        s = CBS('0b10001111001')
        t = s.readlist('pad:1, uint:3, pad:4, uint:3')
        self.assertEqual(t, [0, 1])
        s.pos = 0
        t = s.readlist('pad:1, pad:5')
        self.assertEqual(t, [])
        self.assertEqual(s.pos, 6)
        s.pos = 0
        t = s.readlist('pad:1, bin, pad:4, uint:3')
        self.assertEqual(t, ['000', 1])
        s.pos = 0
        t = s.readlist('pad, bin:3, pad:4, uint:3')
        self.assertEqual(t, ['000', 1])

class ReadingBytes(unittest.TestCase):

    def testUnpackingBytes(self):
        s = CBS(80)
        t = s.unpack('bytes:1')
        self.assertEqual(t[0], b'\x00')
        a, b, c = s.unpack('bytes:1, bytes, bytes:2')
        self.assertEqual(a, b'\x00')
        self.assertEqual(b, b'\x00'*7)
        self.assertEqual(c, b'\x00'*2)

    def testUnpackingBytesWithKeywords(self):
        s = CBS('0x55'*10)
        t = s.unpack('pad:a, bytes:b, bytes, pad:a', a=4, b=6)
        self.assertEqual(t, [b'\x55'*6, b'\x55'*3])

