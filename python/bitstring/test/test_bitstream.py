#!/usr/bin/env python

import unittest
import sys
sys.path.insert(0, '..')
import bitstring
import copy
import os
import collections
from bitstring import BitStream, ConstBitStream, pack
from bitstring import ByteStore, offsetcopy


class FlexibleInitialisation(unittest.TestCase):
    def testFlexibleInitialisation(self):
        a = BitStream('uint:8=12')
        c = BitStream(' uint : 8 =  12')
        self.assertTrue(a == c == BitStream(uint=12, length=8))
        self.assertEqual(a.uint, 12)
        a = BitStream('     int:2=  -1')
        b = BitStream('int :2   = -1')
        c = BitStream(' int:  2  =-1  ')
        self.assertTrue(a == b == c == BitStream(int=-1, length=2))

    def testFlexibleInitialisation2(self):
        h = BitStream('hex=12')
        o = BitStream('oct=33')
        b = BitStream('bin=10')
        self.assertEqual(h, '0x12')
        self.assertEqual(o, '0o33')
        self.assertEqual(b, '0b10')

    def testFlexibleInitialisation3(self):
        for s in ['se=-1', ' se = -1 ', 'se = -1']:
            a = BitStream(s)
            self.assertEqual(a.se, -1)
        for s in ['ue=23', 'ue =23', 'ue = 23']:
            a = BitStream(s)
            self.assertEqual(a.ue, 23)

    def testMultipleStringInitialisation(self):
        a = BitStream('0b1 , 0x1')
        self.assertEqual(a, '0b10001')
        a = BitStream('ue=5, ue=1, se=-2')
        self.assertEqual(a.read('ue'), 5)
        self.assertEqual(a.read('ue'), 1)
        self.assertEqual(a.read('se'), -2)
        b = BitStream('uint:32 = 12, 0b11') + 'int:100=-100, 0o44'
        self.assertEqual(b.read(32).uint, 12)
        self.assertEqual(b.read(2).bin, '11')
        self.assertEqual(b.read(100).int, -100)


class Reading(unittest.TestCase):
    def testReadBits(self):
        s = BitStream(bytes=b'\x4d\x55')
        self.assertEqual(s.read(4).hex, '4')
        self.assertEqual(s.read(8).hex, 'd5')
        self.assertEqual(s.read(1), [0])
        self.assertEqual(s.read(3).bin, '101')
        self.assertFalse(s.read(0))

    def testReadByte(self):
        s = BitStream(hex='4d55')
        self.assertEqual(s.read(8).hex, '4d')
        self.assertEqual(s.read(8).hex, '55')

    def testReadBytes(self):
        s = BitStream(hex='0x112233448811')
        self.assertEqual(s.read(3 * 8).hex, '112233')
        self.assertRaises(ValueError, s.read, -2 * 8)
        s.bitpos += 1
        self.assertEqual(s.read(2 * 8).bin, '1000100100010000')

    def testReadUE(self):
        self.assertRaises(bitstring.InterpretError, BitStream('')._getue)
        # The numbers 0 to 8 as unsigned Exponential-Golomb codes
        s = BitStream(bin='1 010 011 00100 00101 00110 00111 0001000 0001001')
        self.assertEqual(s.pos, 0)
        for i in range(9):
            self.assertEqual(s.read('ue'), i)
        self.assertRaises(bitstring.ReadError, s.read, 'ue')

    def testReadSE(self):
        s = BitStream(bin='010 00110 0001010 0001000 00111')
        self.assertEqual(s.read('se'), 1)
        self.assertEqual(s.read('se'), 3)
        self.assertEqual(s.readlist(3 * ['se']), [5, 4, -3])


class Find(unittest.TestCase):
    def testFind1(self):
        s = ConstBitStream(bin='0b0000110110000')
        self.assertTrue(s.find(BitStream(bin='11011'), False))
        self.assertEqual(s.bitpos, 4)
        self.assertEqual(s.read(5).bin, '11011')
        s.bitpos = 0
        self.assertFalse(s.find('0b11001', False))

    def testFind2(self):
        s = BitStream(bin='0')
        self.assertTrue(s.find(s, False))
        self.assertEqual(s.pos, 0)
        self.assertFalse(s.find('0b00', False))
        self.assertRaises(ValueError, s.find, BitStream(), False)

    def testFindWithOffset(self):
        s = BitStream(hex='0x112233')[4:]
        self.assertTrue(s.find('0x23', False))
        self.assertEqual(s.pos, 8)

    def testFindCornerCases(self):
        s = BitStream(bin='000111000111')
        self.assertTrue(s.find('0b000'))
        self.assertEqual(s.pos, 0)
        self.assertTrue(s.find('0b000'))
        self.assertEqual(s.pos, 0)
        self.assertTrue(s.find('0b0111000111'))
        self.assertEqual(s.pos, 2)
        self.assertTrue(s.find('0b000', start=2))
        self.assertEqual(s.pos, 6)
        self.assertTrue(s.find('0b111', start=6))
        self.assertEqual(s.pos, 9)
        s.pos += 2
        self.assertTrue(s.find('0b1', start=s.pos))

    def testFindBytes(self):
        s = BitStream('0x010203040102ff')
        self.assertFalse(s.find('0x05', bytealigned=True))
        self.assertTrue(s.find('0x02', bytealigned=True))
        self.assertEqual(s.read(16).hex, '0203')
        self.assertTrue(s.find('0x02', start=s.bitpos, bytealigned=True))
        s.read(1)
        self.assertFalse(s.find('0x02', start=s.bitpos, bytealigned=True))

    def testFindBytesAlignedCornerCases(self):
        s = BitStream('0xff')
        self.assertTrue(s.find(s))
        self.assertFalse(s.find(BitStream(hex='0x12')))
        self.assertFalse(s.find(BitStream(hex='0xffff')))

    def testFindBytesBitpos(self):
        s = BitStream(hex='0x1122334455')
        s.pos = 2
        s.find('0x66', bytealigned=True)
        self.assertEqual(s.pos, 2)
        s.pos = 38
        s.find('0x66', bytealigned=True)
        self.assertEqual(s.pos, 38)

    def testFindByteAligned(self):
        s = BitStream(hex='0x12345678')
        self.assertTrue(s.find(BitStream(hex='0x56'), bytealigned=True))
        self.assertEqual(s.bytepos, 2)
        s.pos = 0
        self.assertFalse(s.find(BitStream(hex='0x45'), bytealigned=True))
        s = BitStream('0x1234')
        s.find('0x1234')
        self.assertTrue(s.find('0x1234'))
        s += '0b111'
        s.pos = 3
        s.find('0b1', start=17, bytealigned=True)
        self.assertFalse(s.find('0b1', start=17, bytealigned=True))
        self.assertEqual(s.pos, 3)

    def testFindByteAlignedWithOffset(self):
        s = BitStream(hex='0x112233')[4:]
        self.assertTrue(s.find(BitStream(hex='0x23')))

    def testFindByteAlignedErrors(self):
        s = BitStream(hex='0xffff')
        self.assertRaises(ValueError, s.find, '')
        self.assertRaises(ValueError, s.find, BitStream())


class Rfind(unittest.TestCase):
    def testRfind(self):
        a = BitStream('0b001001001')
        b = a.rfind('0b001')
        self.assertEqual(b, (6,))
        self.assertEqual(a.pos, 6)
        big = BitStream(length=100000) + '0x12' + BitStream(length=10000)
        found = big.rfind('0x12', bytealigned=True)
        self.assertEqual(found, (100000,))
        self.assertEqual(big.pos, 100000)

    def testRfindByteAligned(self):
        a = BitStream('0x8888')
        b = a.rfind('0b1', bytealigned=True)
        self.assertEqual(b, (8,))
        self.assertEqual(a.pos, 8)

    def testRfindStartbit(self):
        a = BitStream('0x0000ffffff')
        b = a.rfind('0x0000', start=1, bytealigned=True)
        self.assertEqual(b, ())
        self.assertEqual(a.pos, 0)
        b = a.rfind('0x00', start=1, bytealigned=True)
        self.assertEqual(b, (8,))
        self.assertEqual(a.pos, 8)

    def testRfindEndbit(self):
        a = BitStream('0x000fff')
        b = a.rfind('0b011', bytealigned=False, start=0, end=14)
        self.assertEqual(bool(b), True)
        b = a.rfind('0b011', False, 0, 13)
        self.assertEqual(b, ())

    def testRfindErrors(self):
        a = BitStream('0x43234234')
        self.assertRaises(ValueError, a.rfind, '', bytealigned=True)
        self.assertRaises(ValueError, a.rfind, '0b1', start=-99, bytealigned=True)
        self.assertRaises(ValueError, a.rfind, '0b1', end=33, bytealigned=True)
        self.assertRaises(ValueError, a.rfind, '0b1', start=10, end=9, bytealigned=True)


class Shift(unittest.TestCase):
    def testShiftLeft(self):
        s = BitStream('0b1010')
        t = s << 1
        self.assertEqual(s.bin, '1010')
        self.assertEqual(t.bin, '0100')
        t = t << 0
        self.assertEqual(t, '0b0100')
        t = t << 100
        self.assertEqual(t.bin, '0000')

    def testShiftLeftErrors(self):
        s = BitStream()
        self.assertRaises(ValueError, s.__lshift__, 1)
        s = BitStream('0xf')
        self.assertRaises(ValueError, s.__lshift__, -1)

    def testShiftRight(self):
        s = BitStream('0b1010')
        t = s >> 1
        self.assertEqual(s.bin, '1010')
        self.assertEqual(t.bin, '0101')
        q = s >> 0
        self.assertEqual(q, '0b1010')
        q.replace('0b1010', '')
        s = s >> 100
        self.assertEqual(s.bin, '0000')

    def testShiftRightErrors(self):
        s = BitStream()
        self.assertRaises(ValueError, s.__rshift__, 1)
        s = BitStream('0xf')
        self.assertRaises(ValueError, s.__rshift__, -1)

    def testShiftRightInPlace(self):
        s = BitStream('0xffff')[4:12]
        s >>= 1
        self.assertEqual(s, '0b01111111')
        s = BitStream('0b11011')
        s >>= 2
        self.assertEqual(s.bin, '00110')
        s >>= 100000000000000
        self.assertEqual(s.bin, '00000')
        s = BitStream('0xff')
        s >>= 1
        self.assertEqual(s, '0x7f')
        s >>= 0
        self.assertEqual(s, '0x7f')

    def testShiftRightInPlaceErrors(self):
        s = BitStream()
        self.assertRaises(ValueError, s.__irshift__, 1)
        s += '0b11'
        self.assertRaises(ValueError, s.__irshift__, -1)

    def testShiftLeftInPlace(self):
        s = BitStream('0xffff')
        t = s[4:12]
        t <<= 2
        self.assertEqual(t, '0b11111100')
        s = BitStream('0b11011')
        s <<= 2
        self.assertEqual(s.bin, '01100')
        s <<= 100000000000000000000
        self.assertEqual(s.bin, '00000')
        s = BitStream('0xff')
        s <<= 1
        self.assertEqual(s, '0xfe')
        s <<= 0
        self.assertEqual(s, '0xfe')

    def testShiftLeftInPlaceErrors(self):
        s = BitStream()
        self.assertRaises(ValueError, s.__ilshift__, 1)
        s += '0b11'
        self.assertRaises(ValueError, s.__ilshift__, -1)


class Replace(unittest.TestCase):
    def testReplace1(self):
        a = BitStream('0b1')
        n = a.replace('0b1', '0b0', bytealigned=True)
        self.assertEqual(a.bin, '0')
        self.assertEqual(n, 1)
        n = a.replace('0b1', '0b0', bytealigned=True)
        self.assertEqual(n, 0)

    def testReplace2(self):
        a = BitStream('0b00001111111')
        n = a.replace('0b1', '0b0', bytealigned=True)
        self.assertEqual(a.bin, '00001111011')
        self.assertEqual(n, 1)
        n = a.replace('0b1', '0b0', bytealigned=False)
        self.assertEqual(a.bin, '00000000000')
        self.assertEqual(n, 6)

    def testReplace3(self):
        a = BitStream('0b0')
        n = a.replace('0b0', '0b110011111', bytealigned=True)
        self.assertEqual(n, 1)
        self.assertEqual(a.bin, '110011111')
        n = a.replace('0b11', '', bytealigned=False)
        self.assertEqual(n, 3)
        self.assertEqual(a.bin, '001')

    def testReplace4(self):
        a = BitStream('0x00114723ef4732344700')
        n = a.replace('0x47', '0x00', bytealigned=True)
        self.assertEqual(n, 3)
        self.assertEqual(a.hex, '00110023ef0032340000')
        a.replace('0x00', '', bytealigned=True)
        self.assertEqual(a.hex, '1123ef3234')
        a.replace('0x11', '', start=1, bytealigned=True)
        self.assertEqual(a.hex, '1123ef3234')
        a.replace('0x11', '0xfff', end=7, bytealigned=True)
        self.assertEqual(a.hex, '1123ef3234')
        a.replace('0x11', '0xfff', end=8, bytealigned=True)
        self.assertEqual(a.hex, 'fff23ef3234')

    def testReplace5(self):
        a = BitStream('0xab')
        b = BitStream('0xcd')
        c = BitStream('0xabef')
        c.replace(a, b)
        self.assertEqual(c, '0xcdef')
        self.assertEqual(a, '0xab')
        self.assertEqual(b, '0xcd')
        a = BitStream('0x0011223344')
        a.pos = 12
        a.replace('0x11', '0xfff', bytealigned=True)
        self.assertEqual(a.pos, 8)
        self.assertEqual(a, '0x00fff223344')

    def testReplaceWithSelf(self):
        a = BitStream('0b11')
        a.replace('0b1', a)
        self.assertEqual(a, '0xf')
        a.replace(a, a)
        self.assertEqual(a, '0xf')

    def testReplaceCount(self):
        a = BitStream('0x223344223344223344')
        n = a.replace('0x2', '0x0', count=0, bytealigned=True)
        self.assertEqual(n, 0)
        self.assertEqual(a.hex, '223344223344223344')
        n = a.replace('0x2', '0x0', count=1, bytealigned=True)
        self.assertEqual(n, 1)
        self.assertEqual(a.hex, '023344223344223344')
        n = a.replace('0x33', '', count=2, bytealigned=True)
        self.assertEqual(n, 2)
        self.assertEqual(a.hex, '02442244223344')
        n = a.replace('0x44', '0x4444', count=1435, bytealigned=True)
        self.assertEqual(n, 3)
        self.assertEqual(a.hex, '02444422444422334444')

    def testReplaceBitpos(self):
        a = BitStream('0xff')
        a.bitpos = 8
        a.replace('0xff', '', bytealigned=True)
        self.assertEqual(a.bitpos, 0)
        a = BitStream('0b0011110001')
        a.bitpos = 4
        a.replace('0b1', '0b000')
        self.assertEqual(a.bitpos, 8)
        a = BitStream('0b1')
        a.bitpos = 1
        a.replace('0b1', '0b11111', bytealigned=True)
        self.assertEqual(a.bitpos, 5)
        a.replace('0b11', '0b0', False)
        self.assertEqual(a.bitpos, 3)
        a.append('0b00')
        a.replace('0b00', '0xffff')
        self.assertEqual(a.bitpos, 17)

    def testReplaceErrors(self):
        a = BitStream('0o123415')
        self.assertRaises(ValueError, a.replace, '', '0o7', bytealigned=True)
        self.assertRaises(ValueError, a.replace, '0b1', '0b1', start=-100, bytealigned=True)
        self.assertRaises(ValueError, a.replace, '0b1', '0b1', end=19, bytealigned=True)


class SliceAssignment(unittest.TestCase):

    # TODO: Move this to another class
    def testSetSlice(self):
        a = BitStream()
        a[0:0] = '0xabcdef'
        self.assertEqual(a.bytepos, 3)
        a[4:16] = ''
        self.assertEqual(a, '0xaef')
        self.assertEqual(a.bitpos, 4)
        a[8:] = '0x00'
        self.assertEqual(a, '0xae00')
        self.assertEqual(a.bytepos, 2)
        a += '0xf'
        a[8:] = '0xe'
        self.assertEqual(a, '0xaee')
        self.assertEqual(a.bitpos, 12)
        b = BitStream()
        b[0:800] = '0xffee'
        self.assertEqual(b, '0xffee')
        b[4:48] = '0xeed123'
        self.assertEqual(b, '0xfeed123')
        b[-800:8] = '0x0000'
        self.assertEqual(b, '0x0000ed123')
        a = BitStream('0xabcde')
        self.assertEqual(a[-100:-90], '')
        self.assertEqual(a[-100:-16], '0xa')
        a[-100:-16] = '0x0'
        self.assertEqual(a, '0x0bcde')

    def testInsertingUsingSetItem(self):
        a = BitStream()
        a[0:0] = '0xdeadbeef'
        self.assertEqual(a, '0xdeadbeef')
        self.assertEqual(a.bytepos, 4)
        a[16:16] = '0xfeed'
        self.assertEqual(a, '0xdeadfeedbeef')
        self.assertEqual(a.bytepos, 4)
        a[0:0] = '0xa'
        self.assertEqual(a, '0xadeadfeedbeef')
        self.assertEqual(a.bitpos, 4)
        a.bytepos = 6
        a[0:0] = '0xff'
        self.assertEqual(a.bytepos, 1)
        a[8:0] = '0x00000'
        self.assertTrue(a.startswith('0xff00000adead'))

    def testSliceAssignmentBitPos(self):
        a = BitStream('int:64=-1')
        a.pos = 64
        a[0:8] = ''
        self.assertEqual(a.pos, 0)
        a.pos = 52
        a[48:56] = '0x0000'
        self.assertEqual(a.pos, 64)
        a[10:10] = '0x0'
        self.assertEqual(a.pos, 14)
        a[56:68] = '0x000'
        self.assertEqual(a.pos, 14)


class Pack(unittest.TestCase):
    def testPack1(self):
        s = bitstring.pack('uint:6, bin, hex, int:6, se, ue, oct', 10, '0b110', 'ff', -1, -6, 6, '54')
        t = BitStream('uint:6=10, 0b110, 0xff, int:6=-1, se=-6, ue=6, oct=54')
        self.assertEqual(s, t)
        self.assertRaises(bitstring.CreationError, pack, 'tomato', '0')
        self.assertRaises(bitstring.CreationError, pack, 'uint', 12)
        self.assertRaises(bitstring.CreationError, pack, 'hex', 'penguin')
        self.assertRaises(bitstring.CreationError, pack, 'hex12', '0x12')

    def testPackWithLiterals(self):
        s = bitstring.pack('0xf')
        self.assertEqual(s, '0xf')
        self.assertTrue(type(s), BitStream)
        s = pack('0b1')
        self.assertEqual(s, '0b1')
        s = pack('0o7')
        self.assertEqual(s, '0o7')
        s = pack('int:10=-1')
        self.assertEqual(s, '0b1111111111')
        s = pack('uint:10=1')
        self.assertEqual(s, '0b0000000001')
        s = pack('ue=12')
        self.assertEqual(s.ue, 12)
        s = pack('se=-12')
        self.assertEqual(s.se, -12)
        s = pack('bin=01')
        self.assertEqual(s.bin, '01')
        s = pack('hex=01')
        self.assertEqual(s.hex, '01')
        s = pack('oct=01')
        self.assertEqual(s.oct, '01')

    def testPackWithDict(self):
        a = pack('uint:6=width, se=height', height=100, width=12)
        w, h = a.unpack('uint:6, se')
        self.assertEqual(w, 12)
        self.assertEqual(h, 100)
        d = {}
        d['w'] = '0xf'
        d['300'] = 423
        d['e'] = '0b1101'
        a = pack('int:100=300, bin=e, uint:12=300', **d)
        x, y, z = a.unpack('int:100, bin, uint:12')
        self.assertEqual(x, 423)
        self.assertEqual(y, '1101')
        self.assertEqual(z, 423)

    def testPackWithDict2(self):
        a = pack('int:5, bin:3=b, 0x3, bin=c, se=12', 10, b='0b111', c='0b1')
        b = BitStream('int:5=10, 0b111, 0x3, 0b1, se=12')
        self.assertEqual(a, b)
        a = pack('bits:3=b', b=BitStream('0b101'))
        self.assertEqual(a, '0b101')
        a = pack('bits:24=b', b=BitStream('0x001122'))
        self.assertEqual(a, '0x001122')

    def testPackWithDict3(self):
        s = pack('hex:4=e, hex:4=0xe, hex:4=e', e='f')
        self.assertEqual(s, '0xfef')
        s = pack('sep', sep='0b00')
        self.assertEqual(s, '0b00')

    def testPackWithDict4(self):
        s = pack('hello', hello='0xf')
        self.assertEqual(s, '0xf')
        s = pack('x, y, x, y, x', x='0b10', y='uint:12=100')
        t = BitStream('0b10, uint:12=100, 0b10, uint:12=100, 0b10')
        self.assertEqual(s, t)
        a = [1, 2, 3, 4, 5]
        s = pack('int:8, div,' * 5, *a, **{'div': '0b1'})
        t = BitStream('int:8=1, 0b1, int:8=2, 0b1, int:8=3, 0b1, int:8=4, 0b1, int:8=5, 0b1')
        self.assertEqual(s, t)

    def testPackWithLocals(self):
        width = 352
        height = 288
        s = pack('uint:12=width, uint:12=height', **locals())
        self.assertEqual(s, '0x160120')

    def testPackWithLengthRestriction(self):
        s = pack('bin:3', '0b000')
        self.assertRaises(bitstring.CreationError, pack, 'bin:3', '0b0011')
        self.assertRaises(bitstring.CreationError, pack, 'bin:3', '0b11')
        self.assertRaises(bitstring.CreationError, pack, 'bin:3=0b0011')
        self.assertRaises(bitstring.CreationError, pack, 'bin:3=0b11')

        s = pack('hex:4', '0xf')
        self.assertRaises(bitstring.CreationError, pack, 'hex:4', '0b111')
        self.assertRaises(bitstring.CreationError, pack, 'hex:4', '0b11111')
        self.assertRaises(bitstring.CreationError, pack, 'hex:8=0xf')

        s = pack('oct:6', '0o77')
        self.assertRaises(bitstring.CreationError, pack, 'oct:6', '0o1')
        self.assertRaises(bitstring.CreationError, pack, 'oct:6', '0o111')
        self.assertRaises(bitstring.CreationError, pack, 'oct:3', '0b1')
        self.assertRaises(bitstring.CreationError, pack, 'oct:3=hello', hello='0o12')

        s = pack('bits:3', BitStream('0b111'))
        self.assertRaises(bitstring.CreationError, pack, 'bits:3', BitStream('0b11'))
        self.assertRaises(bitstring.CreationError, pack, 'bits:3', BitStream('0b1111'))
        self.assertRaises(bitstring.CreationError, pack, 'bits:12=b', b=BitStream('0b11'))

    def testPackNull(self):
        s = pack('')
        self.assertFalse(s)
        s = pack(',')
        self.assertFalse(s)
        s = pack(',,,,,0b1,,,,,,,,,,,,,0b1,,,,,,,,,,')
        self.assertEqual(s, '0b11')
        s = pack(',,uint:12,,bin:3,', 100, '100')
        a, b = s.unpack(',,,uint:12,,,,bin:3,,,')
        self.assertEqual(a, 100)
        self.assertEqual(b, '100')

    def testPackDefaultUint(self):
        s = pack('10, 5', 1, 2)
        a, b = s.unpack('10, 5')
        self.assertEqual((a, b), (1, 2))
        s = pack('10=150, 12=qee', qee=3)
        self.assertEqual(s, 'uint:10=150, uint:12=3')
        t = BitStream('100=5')
        self.assertEqual(t, 'uint:100=5')

    def testPackDefualtUintErrors(self):
        self.assertRaises(bitstring.CreationError, BitStream, '5=-1')

    def testPackingLongKeywordBitstring(self):
        s = pack('bits=b', b=BitStream(128000))
        self.assertEqual(s, BitStream(128000))

    def testPackingWithListFormat(self):
        f = ['bin', 'hex', 'uint:10']
        a = pack(','.join(f), '00', '234', 100)
        b = pack(f, '00', '234', 100)
        self.assertEqual(a, b)


class Unpack(unittest.TestCase):
    def testUnpack1(self):
        s = BitStream('uint:13=23, hex=e, bin=010, int:41=-554, 0o44332, se=-12, ue=4')
        s.pos = 11
        a, b, c, d, e, f, g = s.unpack('uint:13, hex:4, bin:3, int:41, oct:15, se, ue')
        self.assertEqual(a, 23)
        self.assertEqual(b, 'e')
        self.assertEqual(c, '010')
        self.assertEqual(d, -554)
        self.assertEqual(e, '44332')
        self.assertEqual(f, -12)
        self.assertEqual(g, 4)
        self.assertEqual(s.pos, 11)

    def testUnpack2(self):
        s = BitStream('0xff, 0b000, uint:12=100')
        a, b, c = s.unpack('bits:8, bits, uint:12')
        self.assertEqual(type(s), BitStream)
        self.assertEqual(a, '0xff')
        self.assertEqual(type(s), BitStream)
        self.assertEqual(b, '0b000')
        self.assertEqual(c, 100)
        a, b = s.unpack(['bits:11', 'uint'])
        self.assertEqual(a, '0xff, 0b000')
        self.assertEqual(b, 100)

    def testUnpackNull(self):
        s = pack('0b1, , , 0xf,')
        a, b = s.unpack('bin:1,,,hex:4,')
        self.assertEqual(a, '1')
        self.assertEqual(b, 'f')


class FromFile(unittest.TestCase):
    def testCreationFromFileOperations(self):
        s = BitStream(filename='smalltestfile')
        s.append('0xff')
        self.assertEqual(s.hex, '0123456789abcdefff')

        s = ConstBitStream(filename='smalltestfile')
        t = BitStream('0xff') + s
        self.assertEqual(t.hex, 'ff0123456789abcdef')

        s = BitStream(filename='smalltestfile')
        del s[:1]
        self.assertEqual((BitStream('0b0') + s).hex, '0123456789abcdef')

        s = BitStream(filename='smalltestfile')
        del s[:7 * 8]
        self.assertEqual(s.hex, 'ef')

        s = BitStream(filename='smalltestfile')
        s.insert('0xc', 4)
        self.assertEqual(s.hex, '0c123456789abcdef')

        s = BitStream(filename='smalltestfile')
        s.prepend('0xf')
        self.assertEqual(s.hex, 'f0123456789abcdef')

        s = BitStream(filename='smalltestfile')
        s.overwrite('0xaaa', 12)
        self.assertEqual(s.hex, '012aaa6789abcdef')

        s = BitStream(filename='smalltestfile')
        s.reverse()
        self.assertEqual(s.hex, 'f7b3d591e6a2c480')

        s = BitStream(filename='smalltestfile')
        del s[-60:]
        self.assertEqual(s.hex, '0')

        s = BitStream(filename='smalltestfile')
        del s[:60]
        self.assertEqual(s.hex, 'f')

    def testFileProperties(self):
        s = ConstBitStream(filename='smalltestfile')
        self.assertEqual(s.hex, '0123456789abcdef')
        self.assertEqual(s.uint, 81985529216486895)
        self.assertEqual(s.int, 81985529216486895)
        self.assertEqual(s.bin, '0000000100100011010001010110011110001001101010111100110111101111')
        self.assertEqual(s[:-1].oct, '002215053170465363367')
        s.bitpos = 0
        self.assertEqual(s.read('se'), -72)
        s.bitpos = 0
        self.assertEqual(s.read('ue'), 144)
        self.assertEqual(s.bytes, b'\x01\x23\x45\x67\x89\xab\xcd\xef')
        self.assertEqual(s.tobytes(), b'\x01\x23\x45\x67\x89\xab\xcd\xef')

    def testCreationFromFileWithLength(self):
        s = ConstBitStream(filename='test.m1v', length=32)
        self.assertEqual(s.length, 32)
        self.assertEqual(s.hex, '000001b3')
        s = ConstBitStream(filename='test.m1v', length=0)
        self.assertFalse(s)
        self.assertRaises(bitstring.CreationError, BitStream, filename='smalltestfile', length=65)
        self.assertRaises(bitstring.CreationError, ConstBitStream, filename='smalltestfile', length=64, offset=1)
        #        self.assertRaises(bitstring.CreationError, ConstBitStream, filename='smalltestfile', offset=65)
        f = open('smalltestfile', 'rb')
        #        self.assertRaises(bitstring.CreationError, ConstBitStream, auto=f, offset=65)
        self.assertRaises(bitstring.CreationError, ConstBitStream, auto=f, length=65)
        self.assertRaises(bitstring.CreationError, ConstBitStream, auto=f, offset=60, length=5)

    def testCreationFromFileWithOffset(self):
        a = BitStream(filename='test.m1v', offset=4)
        self.assertEqual(a.peek(4 * 8).hex, '00001b31')
        b = BitStream(filename='test.m1v', offset=28)
        self.assertEqual(b.peek(8).hex, '31')

    def testFileSlices(self):
        s = BitStream(filename='smalltestfile')
        self.assertEqual(s[-16:].hex, 'cdef')

    def testCreataionFromFileErrors(self):
        self.assertRaises(IOError, BitStream, filename='Idonotexist')

    def testFindInFile(self):
        s = BitStream(filename='test.m1v')
        self.assertTrue(s.find('0x160120'))
        self.assertEqual(s.bytepos, 4)
        s3 = s.read(3 * 8)
        self.assertEqual(s3.hex, '160120')
        s.bytepos = 0
        self.assertTrue(s._pos == 0)
        self.assertTrue(s.find('0x0001b2'))
        self.assertEqual(s.bytepos, 13)

    def testHexFromFile(self):
        s = BitStream(filename='test.m1v')
        self.assertEqual(s[0:32].hex, '000001b3')
        self.assertEqual(s[-32:].hex, '000001b7')
        s.hex = '0x11'
        self.assertEqual(s.hex, '11')

    def testFileOperations(self):
        s1 = BitStream(filename='test.m1v')
        s2 = BitStream(filename='test.m1v')
        self.assertEqual(s1.read(32).hex, '000001b3')
        self.assertEqual(s2.read(32).hex, '000001b3')
        s1.bytepos += 4
        self.assertEqual(s1.read(8).hex, '02')
        self.assertEqual(s2.read(5 * 8).hex, '1601208302')
        s1.pos = s1.len
        try:
            s1.pos += 1
            self.assertTrue(False)
        except ValueError:
            pass

    def testFileBitGetting(self):
        s = ConstBitStream(filename='smalltestfile', offset=16, length=8) # 0x45
        b = s[1]
        self.assertTrue(b)
        b = s.any(0, [-1, -2, -3])
        self.assertTrue(b)
        b = s.all(0, [0, 1, 2])
        self.assertFalse(b)

    def testVeryLargeFiles(self):
        # This uses an 11GB file which isn't distributed for obvious reasons
        # and so this test won't work for anyone except me!
        try:
            s = ConstBitStream(filename='11GB.mkv')
        except IOError:
            return
        self.assertEqual(s.len, 11743020505 * 8)
        self.assertEqual(s[1000000000:1000000100].hex, 'bdef7335d4545f680d669ce24')
        self.assertEqual(s[-4::8].hex, 'bbebf7a1')


class CreationErrors(unittest.TestCase):
    def testIncorrectBinAssignment(self):
        s = BitStream()
        self.assertRaises(bitstring.CreationError, s._setbin_safe, '0010020')

    def testIncorrectHexAssignment(self):
        s = BitStream()
        self.assertRaises(bitstring.CreationError, s._sethex, '0xabcdefg')


class Length(unittest.TestCase):
    def testLengthZero(self):
        self.assertEqual(BitStream('').len, 0)

    def testLength(self):
        self.assertEqual(BitStream('0x80').len, 8)

    def testLengthErrors(self):
        #TODO: Lots of new checks, for various inits which now disallow length and offset
        pass
        #self.assertRaises(ValueError, BitStream, bin='111', length=-1)
        #self.assertRaises(ValueError, BitStream, bin='111', length=4)

    def testOffsetLengthError(self):
        self.assertRaises(bitstring.CreationError, BitStream, hex='0xffff', offset=-1)


class SimpleConversions(unittest.TestCase):
    def testConvertToUint(self):
        self.assertEqual(BitStream('0x10').uint, 16)
        self.assertEqual(BitStream('0b000111').uint, 7)

    def testConvertToInt(self):
        self.assertEqual(BitStream('0x10').int, 16)
        self.assertEqual(BitStream('0b11110').int, -2)

    def testConvertToHex(self):
        self.assertEqual(BitStream(bytes=b'\x00\x12\x23\xff').hex, '001223ff')
        s = BitStream('0b11111')
        self.assertRaises(bitstring.InterpretError, s._gethex)


class Empty(unittest.TestCase):
    def testEmptyBitstring(self):
        s = BitStream()
        self.assertRaises(bitstring.ReadError, s.read, 1)
        self.assertEqual(s.bin, '')
        self.assertEqual(s.hex, '')
        self.assertRaises(bitstring.InterpretError, s._getint)
        self.assertRaises(bitstring.InterpretError, s._getuint)
        self.assertFalse(s)

    def testNonEmptyBitStream(self):
        s = BitStream(bin='0')
        self.assertFalse(not s.len)


class Position(unittest.TestCase):
    def testBitPosition(self):
        s = BitStream(bytes=b'\x00\x00\x00')
        self.assertEqual(s.bitpos, 0)
        s.read(5)
        self.assertEqual(s.pos, 5)
        s.pos = s.len
        self.assertRaises(bitstring.ReadError, s.read, 1)

    def testBytePosition(self):
        s = BitStream(bytes=b'\x00\x00\x00')
        self.assertEqual(s.bytepos, 0)
        s.read(10)
        self.assertRaises(bitstring.ByteAlignError, s._getbytepos)
        s.read(6)
        self.assertEqual(s.bytepos, 2)

    def testSeekToBit(self):
        s = BitStream(bytes=b'\x00\x00\x00\x00\x00\x00')
        s.bitpos = 0
        self.assertEqual(s.bitpos, 0)
        self.assertRaises(ValueError, s._setbitpos, -1)
        self.assertRaises(ValueError, s._setbitpos, 6 * 8 + 1)
        s.bitpos = 6 * 8
        self.assertEqual(s.bitpos, 6 * 8)

    def testSeekToByte(self):
        s = BitStream(bytes=b'\x00\x00\x00\x00\x00\xab')
        s.bytepos = 5
        self.assertEqual(s.read(8).hex, 'ab')

    def testAdvanceBitsAndBytes(self):
        s = BitStream(bytes=b'\x00\x00\x00\x00\x00\x00\x00\x00')
        s.pos += 5
        self.assertEqual(s.pos, 5)
        s.bitpos += 16
        self.assertEqual(s.pos, 2 * 8 + 5)
        s.pos -= 8
        self.assertEqual(s.pos, 8 + 5)

    def testRetreatBitsAndBytes(self):
        a = BitStream(length=100)
        a.pos = 80
        a.bytepos -= 5
        self.assertEqual(a.bytepos, 5)
        a.pos -= 5
        self.assertEqual(a.pos, 35)


class Offset(unittest.TestCase):
    def testOffset1(self):
        s = BitStream(bytes=b'\x00\x1b\x3f', offset=4)
        self.assertEqual(s.read(8).bin, '00000001')
        self.assertEqual(s.length, 20)

    def testOffset2(self):
        s1 = BitStream(bytes=b'\xf1\x02\x04')
        s2 = BitStream(bytes=b'\xf1\x02\x04', length=23)
        for i in [1, 2, 3, 4, 5, 6, 7, 6, 5, 4, 3, 2, 1, 0, 7, 3, 5, 1, 4]:
            s1._datastore = offsetcopy(s1._datastore, i)
            self.assertEqual(s1.hex, 'f10204')
            s2._datastore = offsetcopy(s2._datastore, i)
            self.assertEqual(s2.bin, '11110001000000100000010')


class Append(unittest.TestCase):
    def testAppend(self):
        s1 = BitStream('0b00000')
        s1.append(BitStream(bool=True))
        self.assertEqual(s1.bin, '000001')
        self.assertEqual((BitStream('0x0102') + BitStream('0x0304')).hex, '01020304')

    def testAppendSameBitstring(self):
        s1 = BitStream('0xf0')[:6]
        s1.append(s1)
        self.assertEqual(s1.bin, '111100111100')

    def testAppendWithOffset(self):
        s = BitStream(bytes=b'\x28\x28', offset=1)
        s.append('0b0')
        self.assertEqual(s.hex, '5050')


class ByteAlign(unittest.TestCase):
    def testByteAlign(self):
        s = BitStream(hex='0001ff23')
        s.bytealign()
        self.assertEqual(s.bytepos, 0)
        s.pos += 11
        s.bytealign()
        self.assertEqual(s.bytepos, 2)
        s.pos -= 10
        s.bytealign()
        self.assertEqual(s.bytepos, 1)

    def testByteAlignWithOffset(self):
        s = BitStream(hex='0112233')
        s._datastore = offsetcopy(s._datastore, 3)
        bitstoalign = s.bytealign()
        self.assertEqual(bitstoalign, 0)
        self.assertEqual(s.read(5).bin, '00001')

    def testInsertByteAligned(self):
        s = BitStream('0x0011')
        s.insert(BitStream('0x22'), 8)
        self.assertEqual(s.hex, '002211')
        s = BitStream(0)
        s.insert(BitStream(bin='101'), 0)
        self.assertEqual(s.bin, '101')


class Truncate(unittest.TestCase):
    def testTruncateStart(self):
        s = BitStream('0b1')
        del s[:1]
        self.assertFalse(s)
        s = BitStream(hex='1234')
        self.assertEqual(s.hex, '1234')
        del s[:4]
        self.assertEqual(s.hex, '234')
        del s[:9]
        self.assertEqual(s.bin, '100')
        del s[:2]
        self.assertEqual(s.bin, '0')
        self.assertEqual(s.len, 1)
        del s[:1]
        self.assertFalse(s)

    def testTruncateEnd(self):
        s = BitStream('0b1')
        del s[-1:]
        self.assertFalse(s)
        s = BitStream(bytes=b'\x12\x34')
        self.assertEqual(s.hex, '1234')
        del s[-4:]
        self.assertEqual(s.hex, '123')
        del s[-9:]
        self.assertEqual(s.bin, '000')
        del s[-3:]
        self.assertFalse(s)
        s = BitStream('0b001')
        del s[:2]
        del s[-1:]
        self.assertFalse(s)


class Slice(unittest.TestCase):
    def testByteAlignedSlice(self):
        s = BitStream(hex='0x123456')
        self.assertEqual(s[8:16].hex, '34')
        s = s[8:24]
        self.assertEqual(s.len, 16)
        self.assertEqual(s.hex, '3456')
        s = s[0:8]
        self.assertEqual(s.hex, '34')
        s.hex = '0x123456'
        self.assertEqual(s[8:24][0:8].hex, '34')

    def testSlice(self):
        s = BitStream(bin='000001111100000')
        s1 = s[0:5]
        s2 = s[5:10]
        s3 = s[10:15]
        self.assertEqual(s1.bin, '00000')
        self.assertEqual(s2.bin, '11111')
        self.assertEqual(s3.bin, '00000')


class Insert(unittest.TestCase):
    def testInsert(self):
        s1 = BitStream(hex='0x123456')
        s2 = BitStream(hex='0xff')
        s1.bytepos = 1
        s1.insert(s2)
        self.assertEqual(s1.bytepos, 2)
        self.assertEqual(s1.hex, '12ff3456')
        s1.insert('0xee', 24)
        self.assertEqual(s1.hex, '12ff34ee56')
        self.assertEqual(s1.bitpos, 32)
        self.assertRaises(ValueError, s1.insert, '0b1', -1000)
        self.assertRaises(ValueError, s1.insert, '0b1', 1000)

    def testInsertNull(self):
        s = BitStream(hex='0x123').insert(BitStream(), 3)
        self.assertEqual(s.hex, '123')

    def testInsertBits(self):
        one = BitStream(bin='1')
        zero = BitStream(bin='0')
        s = BitStream(bin='00')
        s.insert(one, 0)
        self.assertEqual(s.bin, '100')
        s.insert(zero, 0)
        self.assertEqual(s.bin, '0100')
        s.insert(one, s.len)
        self.assertEqual(s.bin, '01001')
        s.insert(s, 2)
        self.assertEqual(s.bin, '0101001001')


class Resetting(unittest.TestCase):
    def testSetHex(self):
        s = BitStream()
        s.hex = '0'
        self.assertEqual(s.hex, '0')
        s.hex = '0x010203045'
        self.assertEqual(s.hex, '010203045')
        self.assertRaises(bitstring.CreationError, s._sethex, '0x002g')

    def testSetBin(self):
        s = BitStream(bin="000101101")
        self.assertEqual(s.bin, '000101101')
        self.assertEqual(s.len, 9)
        s.bin = '0'
        self.assertEqual(s.bin, '0')
        self.assertEqual(s.len, 1)

    def testSetEmptyBin(self):
        s = BitStream(hex='0x000001b3')
        s.bin = ''
        self.assertEqual(s.len, 0)
        self.assertEqual(s.bin, '')

    def testSetInvalidBin(self):
        s = BitStream()
        self.assertRaises(bitstring.CreationError, s._setbin_safe, '00102')


class Overwriting(unittest.TestCase):
    def testOverwriteBit(self):
        s = BitStream(bin='0')
        s.overwrite(BitStream(bin='1'), 0)
        self.assertEqual(s.bin, '1')

    def testOverwriteLimits(self):
        s = BitStream(bin='0b11111')
        s.overwrite(BitStream(bin='000'), 0)
        self.assertEqual(s.bin, '00011')
        s.overwrite('0b000', 2)
        self.assertEqual(s.bin, '00000')

    def testOverwriteNull(self):
        s = BitStream(hex='342563fedec')
        s2 = BitStream(s)
        s.overwrite(BitStream(bin=''), 23)
        self.assertEqual(s.bin, s2.bin)

    def testOverwritePosition(self):
        s1 = BitStream(hex='0123456')
        s2 = BitStream(hex='ff')
        s1.bytepos = 1
        s1.overwrite(s2)
        self.assertEqual((s1.hex, s1.bytepos), ('01ff456', 2))
        s1.overwrite('0xff', 0)
        self.assertEqual((s1.hex, s1.bytepos), ('ffff456', 1))

    def testOverwriteWithSelf(self):
        s = BitStream('0x123')
        s.overwrite(s)
        self.assertEqual(s, '0x123')


class Split(unittest.TestCase):
    def testSplitByteAlignedCornerCases(self):
        s = BitStream()
        bsl = s.split(BitStream(hex='0xff'))
        self.assertEqual(next(bsl).hex, '')
        self.assertRaises(StopIteration, next, bsl)
        s = BitStream(hex='aabbcceeddff')
        delimiter = BitStream()
        bsl = s.split(delimiter)
        self.assertRaises(ValueError, next, bsl)
        delimiter = BitStream(hex='11')
        bsl = s.split(delimiter)
        self.assertEqual(next(bsl).hex, s.hex)

    def testSplitByteAligned(self):
        s = BitStream(hex='0x1234aa1234bbcc1234ffff')
        delimiter = BitStream(hex='1234')
        bsl = s.split(delimiter)
        self.assertEqual([b.hex for b in bsl], ['', '1234aa', '1234bbcc', '1234ffff'])
        self.assertEqual(s.pos, 0)

    def testSplitByteAlignedWithIntialBytes(self):
        s = BitStream(hex='aa471234fedc43 47112233 47 4723 472314')
        delimiter = BitStream(hex='47')
        s.find(delimiter)
        self.assertEqual(s.bytepos, 1)
        bsl = s.split(delimiter, start=0)
        self.assertEqual([b.hex for b in bsl], ['aa', '471234fedc43', '47112233',
                                                '47', '4723', '472314'])
        self.assertEqual(s.bytepos, 1)

    def testSplitByteAlignedWithOverlappingDelimiter(self):
        s = BitStream(hex='aaffaaffaaffaaffaaff')
        bsl = s.split(BitStream(hex='aaffaa'))
        self.assertEqual([b.hex for b in bsl], ['', 'aaffaaff', 'aaffaaffaaff'])


class Adding(unittest.TestCase):
    def testAdding(self):
        s1 = BitStream(hex='0x0102')
        s2 = BitStream(hex='0x0304')
        s3 = s1 + s2
        self.assertEqual(s1.hex, '0102')
        self.assertEqual(s2.hex, '0304')
        self.assertEqual(s3.hex, '01020304')
        s3 += s1
        self.assertEqual(s3.hex, '010203040102')
        self.assertEqual(s2[9:16].bin, '0000100')
        self.assertEqual(s1[0:9].bin, '000000010')
        s4 = BitStream(bin='000000010') +\
             BitStream(bin='0000100')
        self.assertEqual(s4.bin, '0000000100000100')
        s2p = s2[9:16]
        s1p = s1[0:9]
        s5p = s1p + s2p
        s5 = s1[0:9] + s2[9:16]
        self.assertEqual(s5.bin, '0000000100000100')

    def testMoreAdding(self):
        s = BitStream(bin='00') + BitStream(bin='') + BitStream(bin='11')
        self.assertEqual(s.bin, '0011')
        s = '0b01'
        s += BitStream('0b11')
        self.assertEqual(s.bin, '0111')
        s = BitStream('0x00')
        t = BitStream('0x11')
        s += t
        self.assertEqual(s.hex, '0011')
        self.assertEqual(t.hex, '11')
        s += s
        self.assertEqual(s.hex, '00110011')

    def testRadd(self):
        s = '0xff' + BitStream('0xee')
        self.assertEqual(s.hex, 'ffee')


    def testTruncateAsserts(self):
        s = BitStream('0x001122')
        s.bytepos = 2
        del s[-s.len:]
        self.assertEqual(s.bytepos, 0)
        s.append('0x00')
        s.append('0x1122')
        s.bytepos = 2
        del s[:s.len]
        self.assertEqual(s.bytepos, 0)
        s.append('0x00')

    def testOverwriteErrors(self):
        s = BitStream(bin='11111')
        self.assertRaises(ValueError, s.overwrite, BitStream(bin='1'), -10)
        self.assertRaises(ValueError, s.overwrite, BitStream(bin='1'), 6)
        self.assertRaises(ValueError, s.overwrite, BitStream(bin='11111'), 1)

    def testDeleteBits(self):
        s = BitStream(bin='000111100000')
        s.bitpos = 4
        del s[4:8]
        self.assertEqual(s.bin, '00010000')
        del s[4:1004]
        self.assertTrue(s.bin, '0001')

    def testDeleteBitsWithPosition(self):
        s = BitStream(bin='000111100000')
        del s[4:8]
        self.assertEqual(s.bin, '00010000')

    def testDeleteBytes(self):
        s = BitStream('0x00112233')
        del s[8:8]
        self.assertEqual(s.hex, '00112233')
        self.assertEqual(s.pos, 0)
        del s[8:16]
        self.assertEqual(s.hex, '002233')
        self.assertEqual(s.bytepos, 0)
        del s[:24]
        self.assertFalse(s)
        self.assertEqual(s.pos, 0)

    def testGetItemWithPositivePosition(self):
        s = BitStream(bin='0b1011')
        self.assertEqual(s[0], True)
        self.assertEqual(s[1], False)
        self.assertEqual(s[2], True)
        self.assertEqual(s[3], True)
        self.assertRaises(IndexError, s.__getitem__, 4)

    def testGetItemWithNegativePosition(self):
        s = BitStream(bin='1011')
        self.assertEqual(s[-1], True)
        self.assertEqual(s[-2], True)
        self.assertEqual(s[-3], False)
        self.assertEqual(s[-4], True)
        self.assertRaises(IndexError, s.__getitem__, -5)

    def testSlicing(self):
        s = ConstBitStream(hex='0123456789')
        self.assertEqual(s[0:8].hex, '01')
        self.assertFalse(s[0:0])
        self.assertFalse(s[23:20])
        self.assertEqual(s[8:12].bin, '0010')
        self.assertEqual(s[32:80], '0x89')

    def testNegativeSlicing(self):
        s = ConstBitStream(hex='012345678')
        self.assertEqual(s[:-8].hex, '0123456')
        self.assertEqual(s[-16:-8].hex, '56')
        self.assertEqual(s[-24:].hex, '345678')
        self.assertEqual(s[-1000:-24], '0x012')

    def testLen(self):
        s = BitStream()
        self.assertEqual(len(s), 0)
        s.append(BitStream(bin='001'))
        self.assertEqual(len(s), 3)

    def testJoin(self):
        s1 = BitStream(bin='0')
        s2 = BitStream(bin='1')
        s3 = BitStream(bin='000')
        s4 = BitStream(bin='111')
        strings = [s1, s2, s1, s3, s4]
        s = BitStream().join(strings)
        self.assertEqual(s.bin, '010000111')

    def testJoin2(self):
        s1 = BitStream(hex='00112233445566778899aabbccddeeff')
        s2 = BitStream(bin='0b000011')
        bsl = [s1[0:32], s1[4:12], s2, s2, s2, s2]
        s = ConstBitStream().join(bsl)
        self.assertEqual(s.hex, '00112233010c30c3')

        bsl = [BitStream(uint=j, length=12) for j in range(10) for i in range(10)]
        s = BitStream().join(bsl)
        self.assertEqual(s.length, 1200)


    def testPos(self):
        s = BitStream(bin='1')
        self.assertEqual(s.bitpos, 0)
        s.read(1)
        self.assertEqual(s.bitpos, 1)

    def testWritingData(self):
        strings = [BitStream(bin=x) for x in ['0', '001', '0011010010', '010010', '1011']]
        s = BitStream().join(strings)
        s2 = BitStream(bytes=s.bytes)
        self.assertEqual(s2.bin, '000100110100100100101011')
        s2.append(BitStream(bin='1'))
        s3 = BitStream(bytes=s2.tobytes())
        self.assertEqual(s3.bin, '00010011010010010010101110000000')

    def testWritingDataWithOffsets(self):
        s1 = BitStream(bytes=b'\x10')
        s2 = BitStream(bytes=b'\x08\x00', length=8, offset=1)
        s3 = BitStream(bytes=b'\x04\x00', length=8, offset=2)
        self.assertTrue(s1 == s2)
        self.assertTrue(s2 == s3)
        self.assertTrue(s1.bytes == s2.bytes)
        self.assertTrue(s2.bytes == s3.bytes)

    def testVariousThings1(self):
        hexes = ['12345678', '87654321', 'ffffffffff', 'ed', '12ec']
        bins = ['001010', '1101011', '0010000100101110110110', '11', '011']
        bsl = []
        for (hex, bin) in list(zip(hexes, bins)) * 5:
            bsl.append(BitStream(hex=hex))
            bsl.append(BitStream(bin=bin))
        s = BitStream().join(bsl)
        for (hex, bin) in list(zip(hexes, bins)) * 5:
            h = s.read(4 * len(hex))
            b = s.read(len(bin))
            self.assertEqual(h.hex, hex)
            self.assertEqual(b.bin, bin)

    def testVariousThings2(self):
        s1 = BitStream(hex="0x1f08")[:13]
        self.assertEqual(s1.bin, '0001111100001')
        s2 = BitStream(bin='0101')
        self.assertEqual(s2.bin, '0101')
        s1.append(s2)
        self.assertEqual(s1.length, 17)
        self.assertEqual(s1.bin, '00011111000010101')
        s1 = s1[3:8]
        self.assertEqual(s1.bin, '11111')

    def testVariousThings3(self):
        s1 = BitStream(hex='0x012480ff')[2:27]
        s2 = s1 + s1
        self.assertEqual(s2.length, 50)
        s3 = s2[0:25]
        s4 = s2[25:50]
        self.assertEqual(s3.bin, s4.bin)

    def testPeekBit(self):
        s = BitStream(bin='01')
        self.assertEqual(s.peek(1), [0])
        self.assertEqual(s.peek(1), [0])
        self.assertEqual(s.read(1), [0])
        self.assertEqual(s.peek(1), [1])
        self.assertEqual(s.peek(1), [1])

        s = BitStream(bytes=b'\x1f', offset=3)
        self.assertEqual(s.len, 5)
        self.assertEqual(s.peek(5).bin, '11111')
        self.assertEqual(s.peek(5).bin, '11111')
        s.pos += 1
        self.assertRaises(bitstring.ReadError, s.peek, 5)

        s = BitStream(hex='001122334455')
        self.assertEqual(s.peek(8).hex, '00')
        self.assertEqual(s.read(8).hex, '00')
        s.pos += 33
        self.assertRaises(bitstring.ReadError, s.peek, 8)

        s = BitStream(hex='001122334455')
        self.assertEqual(s.peek(8 * 2).hex, '0011')
        self.assertEqual(s.read(8 * 3).hex, '001122')
        self.assertEqual(s.peek(8 * 3).hex, '334455')
        self.assertRaises(bitstring.ReadError, s.peek, 25)

    def testAdvanceBit(self):
        s = BitStream(hex='0xff')
        s.bitpos = 6
        s.pos += 1
        self.assertEqual(s.bitpos, 7)
        s.bitpos += 1
        try:
            s.pos += 1
            self.assertTrue(False)
        except ValueError:
            pass

    def testAdvanceByte(self):
        s = BitStream(hex='0x010203')
        s.bytepos += 1
        self.assertEqual(s.bytepos, 1)
        s.bytepos += 1
        self.assertEqual(s.bytepos, 2)
        s.bytepos += 1
        try:
            s.bytepos += 1
            self.assertTrue(False)
        except ValueError:
            pass

    def testRetreatBit(self):
        s = BitStream(hex='0xff')
        try:
            s.pos -= 1
            self.assertTrue(False)
        except ValueError:
            pass
        s.pos = 5
        s.pos -= 1
        self.assertEqual(s.pos, 4)

    def testRetreatByte(self):
        s = BitStream(hex='0x010203')
        try:
            s.bytepos -= 1
            self.assertTrue(False)
        except ValueError:
            pass
        s.bytepos = 3
        s.bytepos -= 1
        self.assertEqual(s.bytepos, 2)
        self.assertEqual(s.read(8).hex, '03')

    def testCreationByAuto(self):
        s = BitStream('0xff')
        self.assertEqual(s.hex, 'ff')
        s = BitStream('0b00011')
        self.assertEqual(s.bin, '00011')
        self.assertRaises(bitstring.CreationError, BitStream, 'hello')
        s1 = BitStream(bytes=b'\xf5', length=3, offset=5)
        s2 = BitStream(s1, length=1, offset=1)
        self.assertEqual(s2, '0b0')
        s = BitStream(bytes=b'\xff', offset=2)
        t = BitStream(s, offset=2)
        self.assertEqual(t, '0b1111')
        self.assertRaises(TypeError, BitStream, auto=1.2)

    def testCreationByAuto2(self):
        s = BitStream('bin=001')
        self.assertEqual(s.bin, '001')
        s = BitStream('oct=0o007')
        self.assertEqual(s.oct, '007')
        s = BitStream('hex=123abc')
        self.assertEqual(s, '0x123abc')

        s = BitStream('bin:2=01')
        self.assertEqual(s, '0b01')
        for s in ['bin:1=01', 'bits:4=0b1', 'oct:3=000', 'hex:4=0x1234']:
            self.assertRaises(bitstring.CreationError, BitStream, s)

    def testInsertUsingAuto(self):
        s = BitStream('0xff')
        s.insert('0x00', 4)
        self.assertEqual(s.hex, 'f00f')
        self.assertRaises(ValueError, s.insert, 'ff')

    def testOverwriteUsingAuto(self):
        s = BitStream('0x0110')
        s.overwrite('0b1')
        self.assertEqual(s.hex, '8110')
        s.overwrite('')
        self.assertEqual(s.hex, '8110')
        self.assertRaises(ValueError, s.overwrite, '0bf')

    def testFindUsingAuto(self):
        s = BitStream('0b000000010100011000')
        self.assertTrue(s.find('0b101'))
        self.assertEqual(s.pos, 7)

    def testFindbytealignedUsingAuto(self):
        s = BitStream('0x00004700')
        self.assertTrue(s.find('0b01000111', bytealigned=True))
        self.assertEqual(s.bytepos, 2)

    def testAppendUsingAuto(self):
        s = BitStream('0b000')
        s.append('0b111')
        self.assertEqual(s.bin, '000111')
        s.append('0b0')
        self.assertEqual(s.bin, '0001110')

    def testSplitByteAlignedUsingAuto(self):
        s = BitStream('0x000143563200015533000123')
        sections = s.split('0x0001')
        self.assertEqual(next(sections).hex, '')
        self.assertEqual(next(sections).hex, '0001435632')
        self.assertEqual(next(sections).hex, '00015533')
        self.assertEqual(next(sections).hex, '000123')
        self.assertRaises(StopIteration, next, sections)

    def testSplitByteAlignedWithSelf(self):
        s = BitStream('0x1234')
        sections = s.split(s)
        self.assertEqual(next(sections).hex, '')
        self.assertEqual(next(sections).hex, '1234')
        self.assertRaises(StopIteration, next, sections)

    def testPrepend(self):
        s = BitStream('0b000')
        s.prepend('0b11')
        self.assertEqual(s.bin, '11000')
        s.prepend(s)
        self.assertEqual(s.bin, '1100011000')
        s.prepend('')
        self.assertEqual(s.bin, '1100011000')

    def testNullSlice(self):
        s = BitStream('0x111')
        t = s[1:1]
        self.assertEqual(t._datastore.bytelength, 0)

    def testMultipleAutos(self):
        s = BitStream('0xa')
        s.prepend('0xf')
        s.append('0xb')
        self.assertEqual(s, '0xfab')
        s.prepend(s)
        s.append('0x100')
        s.overwrite('0x5', 4)
        self.assertEqual(s, '0xf5bfab100')

    def testReverse(self):
        s = BitStream('0b0011')
        s.reverse()
        self.assertEqual(s.bin, '1100')
        s = BitStream('0b10')
        s.reverse()
        self.assertEqual(s.bin, '01')
        s = BitStream()
        s.reverse()
        self.assertEqual(s.bin, '')

    def testInitWithConcatenatedStrings(self):
        s = BitStream('0xff 0Xee 0xd 0xcc')
        self.assertEqual(s.hex, 'ffeedcc')
        s = BitStream('0b0 0B111 0b001')
        self.assertEqual(s.bin, '0111001')
        s += '0b1' + '0B1'
        self.assertEqual(s.bin, '011100111')
        s = BitStream(hex='ff0xee')
        self.assertEqual(s.hex, 'ffee')
        s = BitStream(bin='000b0b11')
        self.assertEqual(s.bin, '0011')
        s = BitStream('  0o123 0O 7 0   o1')
        self.assertEqual(s.oct, '12371')
        s += '  0 o 332'
        self.assertEqual(s.oct, '12371332')

    def testEquals(self):
        s1 = BitStream('0b01010101')
        s2 = BitStream('0b01010101')
        self.assertTrue(s1 == s2)
        s3 = BitStream()
        s4 = BitStream()
        self.assertTrue(s3 == s4)
        self.assertFalse(s3 != s4)
        s5 = BitStream(bytes=b'\xff', offset=2, length=3)
        s6 = BitStream('0b111')
        self.assertTrue(s5 == s6)
        class A(object):
            pass
        self.assertFalse(s5 == A())

    def testLargeEquals(self):
        s1 = BitStream(1000000)
        s2 = BitStream(1000000)
        s1.set(True, [-1, 55, 53214, 534211, 999999])
        s2.set(True, [-1, 55, 53214, 534211, 999999])
        self.assertEqual(s1, s2)
        s1.set(True, 800000)
        self.assertNotEqual(s1, s2)

    def testNotEquals(self):
        s1 = BitStream('0b0')
        s2 = BitStream('0b1')
        self.assertTrue(s1 != s2)
        self.assertFalse(s1 != BitStream('0b0'))

    def testEqualityWithAutoInitialised(self):
        a = BitStream('0b00110111')
        self.assertTrue(a == '0b00110111')
        self.assertTrue(a == '0x37')
        self.assertTrue('0b0011 0111' == a)
        self.assertTrue('0x3 0x7' == a)
        self.assertFalse(a == '0b11001000')
        self.assertFalse('0x3737' == a)

    def testInvertSpecialMethod(self):
        s = BitStream('0b00011001')
        self.assertEqual((~s).bin, '11100110')
        self.assertEqual((~BitStream('0b0')).bin, '1')
        self.assertEqual((~BitStream('0b1')).bin, '0')
        self.assertTrue(~~s == s)

    def testInvertBitPosition(self):
        s = ConstBitStream('0xefef')
        s.pos = 8
        t = ~s
        self.assertEqual(s.pos, 8)
        self.assertEqual(t.pos, 0)

    def testInvertSpecialMethodErrors(self):
        s = BitStream()
        self.assertRaises(bitstring.Error, s.__invert__)

    def testJoinWithAuto(self):
        s = BitStream().join(['0xf', '0b00', BitStream(bin='11')])
        self.assertEqual(s, '0b11110011')

    def testAutoBitStringCopy(self):
        s = BitStream('0xabcdef')
        t = BitStream(s)
        self.assertEqual(t.hex, 'abcdef')
        del s[-8:]
        self.assertEqual(t.hex, 'abcdef')

class Multiplication(unittest.TestCase):

    def testMultiplication(self):
        a = BitStream('0xff')
        b = a * 8
        self.assertEqual(b, '0xffffffffffffffff')
        b = 4 * a
        self.assertEqual(b, '0xffffffff')
        self.assertTrue(1 * a == a * 1 == a)
        c = a * 0
        self.assertFalse(c)
        a *= 3
        self.assertEqual(a, '0xffffff')
        a *= 0
        self.assertFalse(a)
        one = BitStream('0b1')
        zero = BitStream('0b0')
        mix = one * 2 + 3 * zero + 2 * one * 2
        self.assertEqual(mix, '0b110001111')
        q = BitStream()
        q *= 143
        self.assertFalse(q)
        q += [True, True, False]
        q.pos += 2
        q *= 0
        self.assertFalse(q)
        self.assertEqual(q.bitpos, 0)

    def testMultiplicationWithFiles(self):
        a = BitStream(filename='test.m1v')
        b = a.len
        a *= 3
        self.assertEqual(a.len, 3 * b)

    def testMultiplicationErrors(self):
        a = BitStream('0b1')
        b = BitStream('0b0')
        self.assertRaises(ValueError, a.__mul__, -1)
        self.assertRaises(ValueError, a.__imul__, -1)
        self.assertRaises(ValueError, a.__rmul__, -1)
        self.assertRaises(TypeError, a.__mul__, 1.2)
        self.assertRaises(TypeError, a.__rmul__, b)
        self.assertRaises(TypeError, a.__imul__, b)

    def testFileAndMemEquivalence(self):
        a = ConstBitStream(filename='smalltestfile')
        b = BitStream(filename='smalltestfile')
        self.assertTrue(isinstance(a._datastore._rawarray, bitstring.MmapByteArray))
        self.assertTrue(isinstance(b._datastore._rawarray, bytearray))
        self.assertEqual(a._datastore.getbyte(0), b._datastore.getbyte(0))
        self.assertEqual(a._datastore.getbyteslice(1, 5), bytearray(b._datastore.getbyteslice(1, 5)))


class BitWise(unittest.TestCase):

    def testBitwiseAnd(self):
        a = BitStream('0b01101')
        b = BitStream('0b00110')
        self.assertEqual((a & b).bin, '00100')
        self.assertEqual((a & '0b11111'), a)
        self.assertRaises(ValueError, a.__and__, '0b1')
        self.assertRaises(ValueError, b.__and__, '0b110111111')
        c = BitStream('0b0011011')
        c.pos = 4
        d = c & '0b1111000'
        self.assertEqual(d.pos, 0)
        self.assertEqual(d.bin, '0011000')
        d = '0b1111000' & c
        self.assertEqual(d.bin, '0011000')

    def testBitwiseOr(self):
        a = BitStream('0b111001001')
        b = BitStream('0b011100011')
        self.assertEqual((a | b).bin, '111101011')
        self.assertEqual((a | '0b000000000'), a)
        self.assertRaises(ValueError, a.__or__, '0b0000')
        self.assertRaises(ValueError, b.__or__, a + '0b1')
        a = '0xff00' | BitStream('0x00f0')
        self.assertEqual(a.hex, 'fff0')

    def testBitwiseXor(self):
        a = BitStream('0b111001001')
        b = BitStream('0b011100011')
        self.assertEqual((a ^ b).bin, '100101010')
        self.assertEqual((a ^ '0b111100000').bin, '000101001')
        self.assertRaises(ValueError, a.__xor__, '0b0000')
        self.assertRaises(ValueError, b.__xor__, a + '0b1')
        a = '0o707' ^ BitStream('0o777')
        self.assertEqual(a.oct, '070')

class Split(unittest.TestCase):

    def testSplit(self):
        a = BitStream('0b0 010100111 010100 0101 010')
        a.pos = 20
        subs = [i.bin for i in a.split('0b010')]
        self.assertEqual(subs, ['0', '010100111', '010100', '0101', '010'])
        self.assertEqual(a.pos, 20)

    def testSplitCornerCases(self):
        a = BitStream('0b000000')
        bsl = a.split('0b1', False)
        self.assertEqual(next(bsl), a)
        self.assertRaises(StopIteration, next, bsl)
        b = BitStream()
        bsl = b.split('0b001', False)
        self.assertFalse(next(bsl))
        self.assertRaises(StopIteration, next, bsl)

    def testSplitErrors(self):
        a = BitStream('0b0')
        b = a.split('', False)
        self.assertRaises(ValueError, next, b)

    def testSliceWithOffset(self):
        a = BitStream(bytes=b'\x00\xff\x00', offset=7)
        b = a[7:12]
        self.assertEqual(b.bin, '11000')

    def testSplitWithMaxsplit(self):
        a = BitStream('0xaabbccbbccddbbccddee')
        self.assertEqual(len(list(a.split('0xbb', bytealigned=True))), 4)
        bsl = list(a.split('0xbb', count=1, bytealigned=True))
        self.assertEqual((len(bsl), bsl[0]), (1, '0xaa'))
        bsl = list(a.split('0xbb', count=2, bytealigned=True))
        self.assertEqual(len(bsl), 2)
        self.assertEqual(bsl[0], '0xaa')
        self.assertEqual(bsl[1], '0xbbcc')

    def testSplitMore(self):
        s = BitStream('0b1100011001110110')
        for i in range(10):
            a = list(s.split('0b11', False, count=i))
            b = list(s.split('0b11', False))[:i]
            self.assertEqual(a, b)
        b = s.split('0b11', count=-1)
        self.assertRaises(ValueError, next, b)

    def testSplitStartbit(self):
        a = BitStream('0b0010101001000000001111')
        bsl = a.split('0b001', bytealigned=False, start=1)
        self.assertEqual([x.bin for x in bsl], ['010101', '001000000', '001111'])
        b = a.split('0b001', start=-100)
        self.assertRaises(ValueError, next, b)
        b = a.split('0b001', start=23)
        self.assertRaises(ValueError, next, b)
        b = a.split('0b1', start=10, end=9)
        self.assertRaises(ValueError, next, b)

    def testSplitStartbitByteAligned(self):
        a = BitStream('0x00ffffee')
        bsl = list(a.split('0b111', start=9, bytealigned=True))
        self.assertEqual([x.bin for x in bsl], ['1111111', '11111111', '11101110'])

    def testSplitEndbit(self):
        a = BitStream('0b000010001001011')
        bsl = list(a.split('0b1', bytealigned=False, end=14))
        self.assertEqual([x.bin for x in bsl], ['0000', '1000', '100', '10', '1'])
        self.assertEqual(list(a[4:12].split('0b0', False)), list(a.split('0b0', start=4, end=12)))
        # Shouldn't raise ValueError
        bsl = list(a.split('0xffee', end=15))
        # Whereas this one will when we call next()
        bsl = a.split('0xffee', end=16)
        self.assertRaises(ValueError, next, bsl)

    def testSplitEndbitByteAligned(self):
        a = BitStream('0xff00ff')[:22]
        bsl = list(a.split('0b 0000 0000 111', end=19))
        self.assertEqual([x.bin for x in bsl], ['11111111', '00000000111'])
        bsl = list(a.split('0b 0000 0000 111', end=18))
        self.assertEqual([x.bin for x in bsl], ['111111110000000011'])

    def testSplitMaxSplit(self):
        a = BitStream('0b1' * 20)
        for i in range(10):
            bsl = list(a.split('0b1', count=i))
            self.assertEqual(len(bsl), i)

    #######################

    def testPositionInSlice(self):
        a = BitStream('0x00ffff00')
        a.bytepos = 2
        b = a[8:24]
        self.assertEqual(b.bytepos, 0)

    def testFindByteAlignedWithBits(self):
        a = BitStream('0x00112233445566778899')
        a.find('0b0001', bytealigned=True)
        self.assertEqual(a.bitpos, 8)

    def testFindStartbitNotByteAligned(self):
        a = BitStream('0b0010000100')
        found = a.find('0b1', start=4)
        self.assertEqual((found, a.bitpos), ((7,), 7))
        found = a.find('0b1', start=2)
        self.assertEqual((found, a.bitpos), ((2,), 2))
        found = a.find('0b1', bytealigned=False, start=8)
        self.assertEqual((found, a.bitpos), ((), 2))

    def testFindEndbitNotByteAligned(self):
        a = BitStream('0b0010010000')
        found = a.find('0b1', bytealigned=False, end=2)
        self.assertEqual((found, a.bitpos), ((), 0))
        found = a.find('0b1', end=3)
        self.assertEqual((found, a.bitpos), ((2,), 2))
        found = a.find('0b1', bytealigned=False, start=3, end=5)
        self.assertEqual((found, a.bitpos), ((), 2))
        found = a.find('0b1', start=3, end=6)
        self.assertEqual((found[0], a.bitpos), (5, 5))

    def testFindStartbitByteAligned(self):
        a = BitStream('0xff001122ff0011ff')
        a.pos = 40
        found = a.find('0x22', start=23, bytealigned=True)
        self.assertEqual((found, a.bytepos), ((24,), 3))
        a.bytepos = 4
        found = a.find('0x22', start=24, bytealigned=True)
        self.assertEqual((found, a.bytepos), ((24,), 3))
        found = a.find('0x22', start=25, bytealigned=True)
        self.assertEqual((found, a.pos), ((), 24))
        found = a.find('0b111', start=40, bytealigned=True)
        self.assertEqual((found, a.pos), ((56,), 56))

    def testFindEndbitByteAligned(self):
        a = BitStream('0xff001122ff0011ff')
        found = a.find('0x22', end=31, bytealigned=True)
        self.assertFalse(found)
        self.assertEqual(a.pos, 0)
        found = a.find('0x22', end=32, bytealigned=True)
        self.assertTrue(found)
        self.assertEqual(a.pos, 24)
        self.assertEqual(found[0], 24)

    def testFindStartEndbitErrors(self):
        a = BitStream('0b00100')
        self.assertRaises(ValueError, a.find, '0b1', bytealigned=False, start=-100)
        self.assertRaises(ValueError, a.find, '0b1', end=6)
        self.assertRaises(ValueError, a.find, '0b1', start=4, end=3)
        b = BitStream('0x0011223344')
        self.assertRaises(ValueError, a.find, '0x22', bytealigned=True, start=-100)
        self.assertRaises(ValueError, a.find, '0x22', end=41, bytealigned=True)

    def testPrependAndAppendAgain(self):
        c = BitStream('0x1122334455667788')
        c.bitpos = 40
        c.prepend('0b1')
        self.assertEqual(c.bitpos, 41)
        c = BitStream()
        c.prepend('0x1234')
        self.assertEqual(c.bytepos, 2)
        c = BitStream()
        c.append('0x1234')
        self.assertEqual(c.bytepos, 0)
        s = BitStream(bytes=b'\xff\xff', offset=2)
        self.assertEqual(s.length, 14)
        t = BitStream(bytes=b'\x80', offset=1, length=2)
        s.prepend(t)
        self.assertEqual(s, '0x3fff')

    def testFindAll(self):
        a = BitStream('0b11111')
        p = a.findall('0b1')
        self.assertEqual(list(p), [0, 1, 2, 3, 4])
        p = a.findall('0b11')
        self.assertEqual(list(p), [0, 1, 2, 3])
        p = a.findall('0b10')
        self.assertEqual(list(p), [])
        a = BitStream('0x4733eeff66554747335832434547')
        p = a.findall('0x47', bytealigned=True)
        self.assertEqual(list(p), [0, 6 * 8, 7 * 8, 13 * 8])
        p = a.findall('0x4733', bytealigned=True)
        self.assertEqual(list(p), [0, 7 * 8])
        a = BitStream('0b1001001001001001001')
        p = a.findall('0b1001', bytealigned=False)
        self.assertEqual(list(p), [0, 3, 6, 9, 12, 15])
        self.assertEqual(a.pos, 15)

    def testFindAllGenerator(self):
        a = BitStream('0xff1234512345ff1234ff12ff')
        p = a.findall('0xff', bytealigned=True)
        self.assertEqual(next(p), 0)
        self.assertEqual(next(p), 6 * 8)
        self.assertEqual(next(p), 9 * 8)
        self.assertEqual(next(p), 11 * 8)
        self.assertRaises(StopIteration, next, p)

    def testFindAllCount(self):
        s = BitStream('0b1') * 100
        for i in [0, 1, 23]:
            self.assertEqual(len(list(s.findall('0b1', count=i))), i)
        b = s.findall('0b1', bytealigned=True, count=-1)
        self.assertRaises(ValueError, next, b)

    def testContains(self):
        a = BitStream('0b1') + '0x0001dead0001'
        self.assertTrue('0xdead' in a)
        self.assertEqual(a.pos, 0)
        self.assertFalse('0xfeed' in a)

    def testRepr(self):
        max = bitstring.MAX_CHARS
        bls = ['', '0b1', '0o5', '0x43412424f41', '0b00101001010101']
        for bs in bls:
            a = BitStream(bs)
            b = eval(a.__repr__())
            self.assertTrue(a == b)
        for f in [ConstBitStream(filename='test.m1v'),
                  ConstBitStream(filename='test.m1v', length=17),
                  ConstBitStream(filename='test.m1v', length=23, offset=23102)]:
            f2 = eval(f.__repr__())
            self.assertEqual(f._datastore._rawarray.source.name, f2._datastore._rawarray.source.name)
            self.assertTrue(f2.tobytes() == f.tobytes())
        a = BitStream('0b1')
        self.assertEqual(repr(a), "BitStream('0b1')")
        a += '0b11'
        self.assertEqual(repr(a), "BitStream('0b111')")
        a += '0b1'
        self.assertEqual(repr(a), "BitStream('0xf')")
        a *= max
        self.assertEqual(repr(a), "BitStream('0x" + "f" * max + "')")
        a += '0xf'
        self.assertEqual(repr(a), "BitStream('0x" + "f" * max + "...') # length=%d" % (max * 4 + 4))

    def testPrint(self):
        s = BitStream(hex='0x00')
        self.assertEqual('0x' + s.hex, s.__str__())
        s = BitStream(filename='test.m1v')
        self.assertEqual('0x' + s[0:bitstring.MAX_CHARS * 4].hex + '...', s.__str__())
        self.assertEqual(BitStream().__str__(), '')
        s = BitStream('0b11010')
        self.assertEqual('0b' + s.bin, s.__str__())
        s = BitStream('0x12345678901234567890,0b1')
        self.assertEqual('0x12345678901234567890, 0b1', s.__str__())

    def testIter(self):
        a = BitStream('0b001010')
        b = BitStream()
        for bit in a:
            b.append(ConstBitStream(bool=bit))
        self.assertEqual(a, b)

    def testDelitem(self):
        a = BitStream('0xffee')
        del a[0:8]
        self.assertEqual(a.hex, 'ee')
        del a[0:8]
        self.assertFalse(a)
        del a[10:12]
        self.assertFalse(a)

    def testNonZeroBitsAtStart(self):
        a = BitStream(bytes=b'\xff', offset=2)
        b = BitStream('0b00')
        b += a
        self.assertTrue(b == '0b0011 1111')
        #self.assertEqual(a._datastore.rawbytes, b'\xff')
        self.assertEqual(a.tobytes(), b'\xfc')

    def testNonZeroBitsAtEnd(self):
        a = BitStream(bytes=b'\xff', length=5)
        #self.assertEqual(a._datastore.rawbytes, b'\xff')
        b = BitStream('0b00')
        a += b
        self.assertTrue(a == '0b1111100')
        self.assertEqual(a.tobytes(), b'\xf8')
        self.assertRaises(ValueError, a._getbytes)

    def testNewOffsetErrors(self):
        self.assertRaises(bitstring.CreationError, BitStream, hex='ff', offset=-1)
        self.assertRaises(bitstring.CreationError, BitStream, '0xffffffff', offset=33)

    def testSliceStep(self):
        a = BitStream('0x3')
        b = a[::1]
        self.assertEqual(a, b)
        self.assertEqual(a[2:4:1], '0b11')
        self.assertEqual(a[0:2:1], '0b00')
        self.assertEqual(a[:3], '0o1')

        a = BitStream('0x0011223344556677')
        self.assertEqual(a[-8:], '0x77')
        self.assertEqual(a[:-24], '0x0011223344')
        self.assertEqual(a[-1000:-24], '0x0011223344')

    def testInterestingSliceStep(self):
        a = BitStream('0b0011000111')
        self.assertEqual(a[7:3:-1], '0b1000')
        self.assertEqual(a[9:2:-1], '0b1110001')
        self.assertEqual(a[8:2:-2], '0b100')
        self.assertEqual(a[100:-20:-3], '0b1010')
        self.assertEqual(a[100:-20:-1], '0b1110001100')
        self.assertEqual(a[10:2:-1], '0b1110001')
        self.assertEqual(a[100:2:-1], '0b1110001')

    def testInsertionOrderAndBitpos(self):
        b = BitStream()
        b[0:0] = '0b0'
        b[0:0] = '0b1'
        self.assertEqual(b, '0b10')
        self.assertEqual(b.bitpos, 1)
        a = BitStream()
        a.insert('0b0')
        a.insert('0b1')
        self.assertEqual(a, '0b01')
        self.assertEqual(a.bitpos, 2)

    def testOverwriteOrderAndBitpos(self):
        a = BitStream('0xff')
        a.overwrite('0xa')
        self.assertEqual(a, '0xaf')
        self.assertEqual(a.bitpos, 4)
        a.overwrite('0xb')
        self.assertEqual(a, '0xab')
        self.assertEqual(a.bitpos, 8)
        self.assertRaises(ValueError, a.overwrite, '0b1')
        a.overwrite('0xa', 4)
        self.assertEqual(a, '0xaa')
        self.assertEqual(a.bitpos, 8)
        a.overwrite(a, 0)
        self.assertEqual(a, '0xaa')

    def testInitSliceWithInt(self):
        a = BitStream(length=8)
        a[:] = 100
        self.assertEqual(a.uint, 100)
        a[0] = 1
        self.assertEqual(a.bin, '11100100')
        a[1] = 0
        self.assertEqual(a.bin, '10100100')
        a[-1] = -1
        self.assertEqual(a.bin, '10100101')
        a[-3:] = -2
        self.assertEqual(a.bin, '10100110')

    def testInitSliceWithIntErrors(self):
        a = BitStream('0b0000')
        self.assertRaises(ValueError, a.__setitem__, slice(0, 4), 16)
        self.assertRaises(ValueError, a.__setitem__, slice(0, 4), -9)
        self.assertRaises(ValueError, a.__setitem__, 0, 2)
        self.assertRaises(ValueError, a.__setitem__, 0, -2)

    def testReverseWithSlice(self):
        a = BitStream('0x0012ff')
        a.reverse()
        self.assertEqual(a, '0xff4800')
        a.reverse(8, 16)
        self.assertEqual(a, '0xff1200')
        b = a[8:16]
        b.reverse()
        a[8:16] = b
        self.assertEqual(a, '0xff4800')

    def testReverseWithSliceErrors(self):
        a = BitStream('0x123')
        self.assertRaises(ValueError, a.reverse, -1, 4)
        self.assertRaises(ValueError, a.reverse, 10, 9)
        self.assertRaises(ValueError, a.reverse, 1, 10000)

    def testInitialiseFromList(self):
        a = BitStream([])
        self.assertFalse(a)
        a = BitStream([True, False, [], [0], 'hello'])
        self.assertEqual(a, '0b10011')
        a += []
        self.assertEqual(a, '0b10011')
        a += [True, False, True]
        self.assertEqual(a, '0b10011101')
        a.find([12, 23])
        self.assertEqual(a.pos, 3)
        self.assertEqual([1, 0, False, True], BitStream('0b1001'))
        a = [True] + BitStream('0b1')
        self.assertEqual(a, '0b11')

    def testInitialiseFromTuple(self):
        a = BitStream(())
        self.assertFalse(a)
        a = BitStream((0, 1, '0', '1'))
        self.assertEqual('0b0111', a)
        a.replace((True, True), [])
        self.assertEqual(a, (False, True))

    def testCut(self):
        a = BitStream('0x00112233445')
        b = list(a.cut(8))
        self.assertEqual(b, ['0x00', '0x11', '0x22', '0x33', '0x44'])
        b = list(a.cut(4, 8, 16))
        self.assertEqual(b, ['0x1', '0x1'])
        b = list(a.cut(4, 0, 44, 4))
        self.assertEqual(b, ['0x0', '0x0', '0x1', '0x1'])
        a = BitStream()
        b = list(a.cut(10))
        self.assertTrue(not b)

    def testCutErrors(self):
        a = BitStream('0b1')
        b = a.cut(1, 1, 2)
        self.assertRaises(ValueError, next, b)
        b = a.cut(1, -2, 1)
        self.assertRaises(ValueError, next, b)
        b = a.cut(0)
        self.assertRaises(ValueError, next, b)
        b = a.cut(1, count=-1)
        self.assertRaises(ValueError, next, b)

    def testCutProblem(self):
        s = BitStream('0x1234')
        for n in list(s.cut(4)):
            s.prepend(n)
        self.assertEqual(s, '0x43211234')

    def testJoinFunctions(self):
        a = BitStream().join(['0xa', '0xb', '0b1111'])
        self.assertEqual(a, '0xabf')
        a = BitStream('0b1').join(['0b0' for i in range(10)])
        self.assertEqual(a, '0b0101010101010101010')
        a = BitStream('0xff').join([])
        self.assertFalse(a)

    def testAddingBitpos(self):
        a = BitStream('0xff')
        b = BitStream('0x00')
        a.bitpos = b.bitpos = 8
        c = a + b
        self.assertEqual(c.bitpos, 0)

    def testIntelligentRead1(self):
        a = BitStream(uint=123, length=23)
        u = a.read('uint:23')
        self.assertEqual(u, 123)
        self.assertEqual(a.pos, a.len)
        b = BitStream(int=-12, length=44)
        i = b.read('int:44')
        self.assertEqual(i, -12)
        self.assertEqual(b.pos, b.len)
        u2, i2 = (a + b).readlist('uint:23, int:44')
        self.assertEqual((u2, i2), (123, -12))

    def testIntelligentRead2(self):
        a = BitStream(ue=822)
        u = a.read('ue')
        self.assertEqual(u, 822)
        self.assertEqual(a.pos, a.len)
        b = BitStream(se=-1001)
        s = b.read('se')
        self.assertEqual(s, -1001)
        self.assertEqual(b.pos, b.len)
        s, u1, u2 = (b + 2 * a).readlist('se, ue, ue')
        self.assertEqual((s, u1, u2), (-1001, 822, 822))

    def testIntelligentRead3(self):
        a = BitStream('0x123') + '0b11101'
        h = a.read('hex:12')
        self.assertEqual(h, '123')
        b = a.read('bin: 5')
        self.assertEqual(b, '11101')
        c = '0b' + b + a
        b, h = c.readlist('bin:5, hex:12')
        self.assertEqual((b, h), ('11101', '123'))

    def testIntelligentRead4(self):
        a = BitStream('0o007')
        o = a.read('oct:9')
        self.assertEqual(o, '007')
        self.assertEqual(a.pos, a.len)

    def testIntelligentRead5(self):
        a = BitStream('0x00112233')
        c0, c1, c2 = a.readlist('bits:8, bits:8, bits:16')
        self.assertEqual((c0, c1, c2), (BitStream('0x00'), BitStream('0x11'), BitStream('0x2233')))
        a.pos = 0
        c = a.read('bits:16')
        self.assertEqual(c, BitStream('0x0011'))

    def testIntelligentRead6(self):
        a = BitStream('0b000111000')
        b1, b2, b3 = a.readlist('bin :3, int: 3, int:3')
        self.assertEqual(b1, '000')
        self.assertEqual(b2, -1)
        self.assertEqual(b3, 0)

    def testIntelligentRead7(self):
        a = BitStream('0x1234')
        a1, a2, a3, a4 = a.readlist('bin:0, oct:0, hex:0, bits:0')
        self.assertTrue(a1 == a2 == a3 == '')
        self.assertFalse(a4)
        self.assertRaises(ValueError, a.read, 'int:0')
        self.assertRaises(ValueError, a.read, 'uint:0')
        self.assertEqual(a.pos, 0)

    def testIntelligentRead8(self):
        a = BitStream('0x123456')
        for t in ['hex:1', 'oct:1', 'hex4', '-5', 'fred', 'bin:-2',
                  'uint:p', 'uint:-2', 'int:u', 'int:-3', 'ses', 'uee', '-14']:
            self.assertRaises(ValueError, a.read, t)

    def testIntelligentRead9(self):
        a = BitStream('0xff')
        self.assertEqual(a.read('intle'), -1)

    def testFillerReads1(self):
        s = BitStream('0x012345')
        t = s.read('bits')
        self.assertEqual(s, t)
        s.pos = 0
        a, b = s.readlist('hex:8, hex')
        self.assertEqual(a, '01')
        self.assertEqual(b, '2345')
        self.assertTrue(isinstance(b, str))
        s.bytepos = 0
        a, b = s.readlist('bin, hex:20')
        self.assertEqual(a, '0000')
        self.assertEqual(b, '12345')
        self.assertTrue(isinstance(a, str))

    def testFillerReads2(self):
        s = BitStream('0xabcdef')
        self.assertRaises(bitstring.Error, s.readlist, 'bits, se')
        self.assertRaises(bitstring.Error, s.readlist, 'hex:4, bits, ue, bin:4')
        s.pos = 0
        self.assertRaises(bitstring.Error, s.readlist, 'bin, bin')

    def testIntelligentPeek(self):
        a = BitStream('0b01, 0x43, 0o4, uint:23=2, se=5, ue=3')
        b, c, e = a.peeklist('bin:2, hex:8, oct:3')
        self.assertEqual((b, c, e), ('01', '43', '4'))
        self.assertEqual(a.pos, 0)
        a.pos = 13
        f, g, h = a.peeklist('uint:23, se, ue')
        self.assertEqual((f, g, h), (2, 5, 3))
        self.assertEqual(a.pos, 13)

    def testReadMultipleBits(self):
        s = BitStream('0x123456789abcdef')
        a, b = s.readlist([4, 4])
        self.assertEqual(a, '0x1')
        self.assertEqual(b, '0x2')
        c, d, e = s.readlist([8, 16, 8])
        self.assertEqual(c, '0x34')
        self.assertEqual(d, '0x5678')
        self.assertEqual(e, '0x9a')

    def testPeekMultipleBits(self):
        s = BitStream('0b1101, 0o721, 0x2234567')
        a, b, c, d = s.peeklist([2, 1, 1, 9])
        self.assertEqual(a, '0b11')
        self.assertEqual(bool(b), False)
        self.assertEqual(bool(c), True)
        self.assertEqual(d, '0o721')
        self.assertEqual(s.pos, 0)
        a, b = s.peeklist([4, 9])
        self.assertEqual(a, '0b1101')
        self.assertEqual(b, '0o721')
        s.pos = 13
        a, b = s.peeklist([16, 8])
        self.assertEqual(a, '0x2234')
        self.assertEqual(b, '0x56')
        self.assertEqual(s.pos, 13)

    def testDifficultPrepends(self):
        a = BitStream('0b1101011')
        b = BitStream()
        for i in range(10):
            b.prepend(a)
        self.assertEqual(b, a * 10)

    def testPackingWrongNumberOfThings(self):
        self.assertRaises(bitstring.CreationError, pack, 'bin:1')
        self.assertRaises(bitstring.CreationError, pack, '', 100)

    def testPackWithVariousKeys(self):
        a = pack('uint10', uint10='0b1')
        self.assertEqual(a, '0b1')
        b = pack('0b110', **{'0b110': '0xfff'})
        self.assertEqual(b, '0xfff')

    def testPackWithVariableLength(self):
        for i in range(1, 11):
            a = pack('uint:n', 0, n=i)
            self.assertEqual(a.bin, '0' * i)

    def testToBytes(self):
        a = BitStream(bytes=b'\xab\x00')
        b = a.tobytes()
        self.assertEqual(a.bytes, b)
        for i in range(7):
            del a[-1:]
            self.assertEqual(a.tobytes(), b'\xab\x00')
        del a[-1:]
        self.assertEqual(a.tobytes(), b'\xab')

    def testToFile(self):
        a = BitStream('0x0000ff')[:17]
        f = open('temp_bitstring_unit_testing_file', 'wb')
        a.tofile(f)
        f.close()
        b = BitStream(filename='temp_bitstring_unit_testing_file')
        self.assertEqual(b, '0x000080')

        a = BitStream('0x911111')
        del a[:1]
        self.assertEqual(a + '0b0', '0x222222')
        f = open('temp_bitstring_unit_testing_file', 'wb')
        a.tofile(f)
        f.close()
        b = BitStream(filename='temp_bitstring_unit_testing_file')
        self.assertEqual(b, '0x222222')
        os.remove('temp_bitstring_unit_testing_file')

    #def testToFileWithLargerFile(self):
    #    a = BitStream(length=16000000)
    #    a[1] = '0b1'
    #    a[-2] = '0b1'
    #    f = open('temp_bitstring_unit_testing_file' ,'wb')
    #    a.tofile(f)
    #    f.close()
    #    b = BitStream(filename='temp_bitstring_unit_testing_file')
    #    self.assertEqual(b.len, 16000000)
    #    self.assertEqual(b[1], True)
    #
    #    f = open('temp_bitstring_unit_testing_file' ,'wb')
    #    a[1:].tofile(f)
    #    f.close()
    #    b = BitStream(filename='temp_bitstring_unit_testing_file')
    #    self.assertEqual(b.len, 16000000)
    #    self.assertEqual(b[0], True)
    #    os.remove('temp_bitstring_unit_testing_file')

    def testTokenParser(self):
        tp = bitstring.tokenparser
        self.assertEqual(tp('hex'), (True, [('hex', None, None)]))
        self.assertEqual(tp('hex=14'), (True, [('hex', None, '14')]))
        self.assertEqual(tp('se'), (False, [('se', None, None)]))
        self.assertEqual(tp('ue=12'), (False, [('ue', None, '12')]))
        self.assertEqual(tp('0xef'), (False, [('0x', None, 'ef')]))
        self.assertEqual(tp('uint:12'), (False, [('uint', 12, None)]))
        self.assertEqual(tp('int:30=-1'), (False, [('int', 30, '-1')]))
        self.assertEqual(tp('bits:10'), (False, [('bits', 10, None)]))
        self.assertEqual(tp('bits:10'), (False, [('bits', 10, None)]))
        self.assertEqual(tp('123'), (False, [('uint', 123, None)]))
        self.assertEqual(tp('123'), (False, [('uint', 123, None)]))
        self.assertRaises(ValueError, tp, 'hex12')
        self.assertEqual(tp('hex12', ('hex12',)), (False, [('hex12', None, None)]))
        self.assertEqual(tp('2*bits:6'), (False, [('bits', 6, None), ('bits', 6, None)]))

    def testAutoFromFileObject(self):
        with open('test.m1v', 'rb') as f:
            s = ConstBitStream(f, offset=32, length=12)
            self.assertEqual(s.uint, 352)
            t = ConstBitStream('0xf') + f
            self.assertTrue(t.startswith('0xf000001b3160'))
            s2 = ConstBitStream(f)
            t2 = BitStream('0xc')
            t2.prepend(s2)
            self.assertTrue(t2.startswith('0x000001b3'))
            self.assertTrue(t2.endswith('0xc'))
            with open('test.m1v', 'rb') as b:
                u = BitStream(bytes=b.read())
                # TODO: u == s2 is much slower than u.bytes == s2.bytes
                self.assertEqual(u.bytes, s2.bytes)

    def testFileBasedCopy(self):
        with open('smalltestfile', 'rb') as f:
            s = BitStream(f)
            t = BitStream(s)
            s.prepend('0b1')
            self.assertEqual(s[1:], t)
            s = BitStream(f)
            t = copy.copy(s)
            t.append('0b1')
            self.assertEqual(s, t[:-1])

    def testBigEndianSynonyms(self):
        s = BitStream('0x12318276ef')
        self.assertEqual(s.int, s.intbe)
        self.assertEqual(s.uint, s.uintbe)
        s = BitStream(intbe=-100, length=16)
        self.assertEqual(s, 'int:16=-100')
        s = BitStream(uintbe=13, length=24)
        self.assertEqual(s, 'int:24=13')
        s = BitStream('uintbe:32=1000')
        self.assertEqual(s, 'uint:32=1000')
        s = BitStream('intbe:8=2')
        self.assertEqual(s, 'int:8=2')
        self.assertEqual(s.read('intbe'), 2)
        s.pos = 0
        self.assertEqual(s.read('uintbe'), 2)

    def testBigEndianSynonymErrors(self):
        self.assertRaises(bitstring.CreationError, BitStream, uintbe=100, length=15)
        self.assertRaises(bitstring.CreationError, BitStream, intbe=100, length=15)
        self.assertRaises(bitstring.CreationError, BitStream, 'uintbe:17=100')
        self.assertRaises(bitstring.CreationError, BitStream, 'intbe:7=2')
        s = BitStream('0b1')
        self.assertRaises(bitstring.InterpretError, s._getintbe)
        self.assertRaises(bitstring.InterpretError, s._getuintbe)
        self.assertRaises(ValueError, s.read, 'uintbe')
        self.assertRaises(ValueError, s.read, 'intbe')

    def testLittleEndianUint(self):
        s = BitStream(uint=100, length=16)
        self.assertEqual(s.uintle, 25600)
        s = BitStream(uintle=100, length=16)
        self.assertEqual(s.uint, 25600)
        self.assertEqual(s.uintle, 100)
        s.uintle += 5
        self.assertEqual(s.uintle, 105)
        s = BitStream('uintle:32=999')
        self.assertEqual(s.uintle, 999)
        s.byteswap()
        self.assertEqual(s.uint, 999)
        s = pack('uintle:24', 1001)
        self.assertEqual(s.uintle, 1001)
        self.assertEqual(s.length, 24)
        self.assertEqual(s.read('uintle'), 1001)

    def testLittleEndianInt(self):
        s = BitStream(int=100, length=16)
        self.assertEqual(s.intle, 25600)
        s = BitStream(intle=100, length=16)
        self.assertEqual(s.int, 25600)
        self.assertEqual(s.intle, 100)
        s.intle += 5
        self.assertEqual(s.intle, 105)
        s = BitStream('intle:32=999')
        self.assertEqual(s.intle, 999)
        s.byteswap()
        self.assertEqual(s.int, 999)
        s = pack('intle:24', 1001)
        self.assertEqual(s.intle, 1001)
        self.assertEqual(s.length, 24)
        self.assertEqual(s.read('intle'), 1001)

    def testLittleEndianErrors(self):
        self.assertRaises(bitstring.CreationError, BitStream, 'uintle:15=10')
        self.assertRaises(bitstring.CreationError, BitStream, 'intle:31=-999')
        self.assertRaises(bitstring.CreationError, BitStream, uintle=100, length=15)
        self.assertRaises(bitstring.CreationError, BitStream, intle=100, length=15)
        s = BitStream('0xfff')
        self.assertRaises(bitstring.InterpretError, s._getintle)
        self.assertRaises(bitstring.InterpretError, s._getuintle)
        self.assertRaises(ValueError, s.read, 'uintle')
        self.assertRaises(ValueError, s.read, 'intle')

    def testStructTokens1(self):
        self.assertEqual(pack('<b', 23), BitStream('intle:8=23'))
        self.assertEqual(pack('<B', 23), BitStream('uintle:8=23'))
        self.assertEqual(pack('<h', 23), BitStream('intle:16=23'))
        self.assertEqual(pack('<H', 23), BitStream('uintle:16=23'))
        self.assertEqual(pack('<l', 23), BitStream('intle:32=23'))
        self.assertEqual(pack('<L', 23), BitStream('uintle:32=23'))
        self.assertEqual(pack('<q', 23), BitStream('intle:64=23'))
        self.assertEqual(pack('<Q', 23), BitStream('uintle:64=23'))
        self.assertEqual(pack('>b', 23), BitStream('intbe:8=23'))
        self.assertEqual(pack('>B', 23), BitStream('uintbe:8=23'))
        self.assertEqual(pack('>h', 23), BitStream('intbe:16=23'))
        self.assertEqual(pack('>H', 23), BitStream('uintbe:16=23'))
        self.assertEqual(pack('>l', 23), BitStream('intbe:32=23'))
        self.assertEqual(pack('>L', 23), BitStream('uintbe:32=23'))
        self.assertEqual(pack('>q', 23), BitStream('intbe:64=23'))
        self.assertEqual(pack('>Q', 23), BitStream('uintbe:64=23'))
        self.assertRaises(bitstring.CreationError, pack, '<B', -1)
        self.assertRaises(bitstring.CreationError, pack, '<H', -1)
        self.assertRaises(bitstring.CreationError, pack, '<L', -1)
        self.assertRaises(bitstring.CreationError, pack, '<Q', -1)

    def testStructTokens2(self):
        endianness = sys.byteorder
        sys.byteorder = 'little'
        self.assertEqual(pack('@b', 23), BitStream('intle:8=23'))
        self.assertEqual(pack('@B', 23), BitStream('uintle:8=23'))
        self.assertEqual(pack('@h', 23), BitStream('intle:16=23'))
        self.assertEqual(pack('@H', 23), BitStream('uintle:16=23'))
        self.assertEqual(pack('@l', 23), BitStream('intle:32=23'))
        self.assertEqual(pack('@L', 23), BitStream('uintle:32=23'))
        self.assertEqual(pack('@q', 23), BitStream('intle:64=23'))
        self.assertEqual(pack('@Q', 23), BitStream('uintle:64=23'))
        sys.byteorder = 'big'
        self.assertEqual(pack('@b', 23), BitStream('intbe:8=23'))
        self.assertEqual(pack('@B', 23), BitStream('uintbe:8=23'))
        self.assertEqual(pack('@h', 23), BitStream('intbe:16=23'))
        self.assertEqual(pack('@H', 23), BitStream('uintbe:16=23'))
        self.assertEqual(pack('@l', 23), BitStream('intbe:32=23'))
        self.assertEqual(pack('@L', 23), BitStream('uintbe:32=23'))
        self.assertEqual(pack('@q', 23), BitStream('intbe:64=23'))
        self.assertEqual(pack('@Q', 23), BitStream('uintbe:64=23'))
        sys.byteorder = endianness

    def testNativeEndianness(self):
        s = pack('@2L', 40, 40)
        if sys.byteorder == 'little':
            self.assertEqual(s, pack('<2L', 40, 40))
        else:
            self.assertEqual(sys.byteorder, 'big')
            self.assertEqual(s, pack('>2L', 40, 40))

    def testStructTokens2(self):
        s = pack('>hhl', 1, 2, 3)
        a, b, c = s.unpack('>hhl')
        self.assertEqual((a, b, c), (1, 2, 3))
        s = pack('<QL, >Q \tL', 1001, 43, 21, 9999)
        self.assertEqual(s.unpack('<QL, >QL'), [1001, 43, 21, 9999])

    def testStructTokensMultiplicativeFactors(self):
        s = pack('<2h', 1, 2)
        a, b = s.unpack('<2h')
        self.assertEqual((a, b), (1, 2))
        s = pack('<100q', *range(100))
        self.assertEqual(s.len, 100 * 64)
        self.assertEqual(s[44*64:45*64].uintle, 44)
        s = pack('@L0B2h', 5, 5, 5)
        self.assertEqual(s.unpack('@Lhh'), [5, 5, 5])

    def testStructTokensErrors(self):
        for f in ['>>q', '<>q', 'q>', '2q', 'q', '>-2q', '@a', '>int:8', '>q2']:
            self.assertRaises(bitstring.CreationError, pack, f, 100)

    def testImmutableBitStreams(self):
        a = ConstBitStream('0x012345')
        self.assertEqual(a, '0x012345')
        b = BitStream('0xf') + a
        self.assertEqual(b, '0xf012345')
        try:
            a.append(b)
            self.assertTrue(False)
        except AttributeError:
            pass
        try:
            a.prepend(b)
            self.assertTrue(False)
        except AttributeError:
            pass
        try:
            a[0] = '0b1'
            self.assertTrue(False)
        except TypeError:
            pass
        try:
            del a[5]
            self.assertTrue(False)
        except TypeError:
            pass
        try:
            a.replace('0b1', '0b0')
            self.assertTrue(False)
        except AttributeError:
            pass
        try:
            a.insert('0b11', 4)
            self.assertTrue(False)
        except AttributeError:
            pass
        try:
            a.reverse()
            self.assertTrue(False)
        except AttributeError:
            pass
        try:
            a.reversebytes()
            self.assertTrue(False)
        except AttributeError:
            pass
        self.assertEqual(a, '0x012345')
        self.assertTrue(isinstance(a, ConstBitStream))

    def testReverseBytes(self):
        a = BitStream('0x123456')
        a.byteswap()
        self.assertEqual(a, '0x563412')
        b = a + '0b1'
        b.byteswap()
        self.assertEqual('0x123456, 0b1', b)
        a = BitStream('0x54')
        a.byteswap()
        self.assertEqual(a, '0x54')
        a = BitStream()
        a.byteswap()
        self.assertFalse(a)

    def testReverseBytes2(self):
        a = BitStream()
        a.byteswap()
        self.assertFalse(a)
        a = BitStream('0x00112233')
        a.byteswap(0, 0, 16)
        self.assertEqual(a, '0x11002233')
        a.byteswap(0, 4, 28)
        self.assertEqual(a, '0x12302103')
        a.byteswap(start=0, end=18)
        self.assertEqual(a, '0x30122103')
        self.assertRaises(ValueError, a.byteswap, 0, 10, 2)
        self.assertRaises(ValueError, a.byteswap, 0, -4, 4)
        self.assertRaises(ValueError, a.byteswap, 0, 24, 48)
        a.byteswap(0, 24)
        self.assertEqual(a, '0x30122103')
        a.byteswap(0, 11, 11)
        self.assertEqual(a, '0x30122103')

    def testCapitalsInPack(self):
        a = pack('A', A='0b1')
        self.assertEqual(a, '0b1')
        format = 'bits:4=BL_OFFT, uint:12=width, uint:12=height'
        d = {'BL_OFFT': '0b1011', 'width': 352, 'height': 288}
        s = bitstring.pack(format, **d)
        self.assertEqual(s, '0b1011, uint:12=352, uint:12=288')
        a = pack('0X0, uint:8, hex', 45, '0XABcD')
        self.assertEqual(a, '0x0, uint:8=45, 0xabCD')

    def testOtherCapitals(self):
        a = ConstBitStream('0XABC, 0O0, 0B11')
        self.assertEqual(a, 'hex=0Xabc, oct=0, bin=0B11')

    def testEfficientOverwrite(self):
        a = BitStream(1000000000)
        a.overwrite([1], 123456)
        self.assertEqual(a[123456], True)
        a.overwrite('0xff', 1)
        self.assertEqual(a[0:32:1], '0x7f800000')
        b = BitStream('0xffff')
        b.overwrite('0x0000')
        self.assertEqual(b, '0x0000')
        self.assertEqual(b.pos, 16)
        c = BitStream(length=1000)
        c.overwrite('0xaaaaaaaaaaaa', 81)
        self.assertEqual(c[81:81 + 6 * 8], '0xaaaaaaaaaaaa')
        self.assertEqual(len(list(c.findall('0b1'))), 24)
        s = BitStream(length=1000)
        s = s[5:]
        s.overwrite('0xffffff', 500)
        s.pos = 500
        self.assertEqual(s.read(4 * 8), '0xffffff00')
        s.overwrite('0xff', 502)
        self.assertEqual(s[502:518], '0xffff')

    def testPeekAndReadListErrors(self):
        a = BitStream('0x123456')
        self.assertRaises(ValueError, a.read, 'hex:8, hex:8')
        self.assertRaises(ValueError, a.peek, 'hex:8, hex:8')
        self.assertRaises(TypeError, a.read, 10, 12)
        self.assertRaises(TypeError, a.peek, 12, 14)
        self.assertRaises(TypeError, a.read, 8, 8)
        self.assertRaises(TypeError, a.peek, 80, 80)

    def testStartswith(self):
        a = BitStream()
        self.assertTrue(a.startswith(BitStream()))
        self.assertFalse(a.startswith('0b0'))
        a = BitStream('0x12ff')
        self.assertTrue(a.startswith('0x1'))
        self.assertTrue(a.startswith('0b0001001'))
        self.assertTrue(a.startswith('0x12ff'))
        self.assertFalse(a.startswith('0x12ff, 0b1'))
        self.assertFalse(a.startswith('0x2'))

    def testStartswithStartEnd(self):
        s = BitStream('0x123456')
        self.assertTrue(s.startswith('0x234', 4))
        self.assertFalse(s.startswith('0x123', end=11))
        self.assertTrue(s.startswith('0x123', end=12))
        self.assertTrue(s.startswith('0x34', 8, 16))
        self.assertFalse(s.startswith('0x34', 7, 16))
        self.assertFalse(s.startswith('0x34', 9, 16))
        self.assertFalse(s.startswith('0x34', 8, 15))

    def testEndswith(self):
        a = BitStream()
        self.assertTrue(a.endswith(''))
        self.assertFalse(a.endswith(BitStream('0b1')))
        a = BitStream('0xf2341')
        self.assertTrue(a.endswith('0x41'))
        self.assertTrue(a.endswith('0b001'))
        self.assertTrue(a.endswith('0xf2341'))
        self.assertFalse(a.endswith('0x1f2341'))
        self.assertFalse(a.endswith('0o34'))

    def testEndswithStartEnd(self):
        s = BitStream('0x123456')
        self.assertTrue(s.endswith('0x234', end=16))
        self.assertFalse(s.endswith('0x456', start=13))
        self.assertTrue(s.endswith('0x456', start=12))
        self.assertTrue(s.endswith('0x34', 8, 16))
        self.assertTrue(s.endswith('0x34', 7, 16))
        self.assertFalse(s.endswith('0x34', 9, 16))
        self.assertFalse(s.endswith('0x34', 8, 15))

    def testUnhashability(self):
        s = BitStream('0xf')
        self.assertRaises(TypeError, set, [s])
        self.assertRaises(TypeError, hash, [s])

    def testConstBitStreamSetCreation(self):
        sl = [ConstBitStream(uint=i, length=7) for i in range(15)]
        s = set(sl)
        self.assertEqual(len(s), 15)
        s.add(ConstBitStream('0b0000011'))
        self.assertEqual(len(s), 15)
        self.assertRaises(TypeError, s.add, BitStream('0b0000011'))

    def testConstBitStreamFunctions(self):
        s = ConstBitStream('0xf, 0b1')
        self.assertEqual(type(s), ConstBitStream)
        t = copy.copy(s)
        self.assertEqual(type(t), ConstBitStream)
        a = s + '0o3'
        self.assertEqual(type(a), ConstBitStream)
        b = a[0:4]
        self.assertEqual(type(b), ConstBitStream)
        b = a[4:3]
        self.assertEqual(type(b), ConstBitStream)
        b = a[5:2:-1]
        self.assertEqual(type(b), ConstBitStream)
        b = ~a
        self.assertEqual(type(b), ConstBitStream)
        b = a << 2
        self.assertEqual(type(b), ConstBitStream)
        b = a >> 2
        self.assertEqual(type(b), ConstBitStream)
        b = a * 2
        self.assertEqual(type(b), ConstBitStream)
        b = a * 0
        self.assertEqual(type(b), ConstBitStream)
        b = a & ~a
        self.assertEqual(type(b), ConstBitStream)
        b = a | ~a
        self.assertEqual(type(b), ConstBitStream)
        b = a ^ ~a
        self.assertEqual(type(b), ConstBitStream)
        b = a._slice(4, 4)
        self.assertEqual(type(b), ConstBitStream)
        b = a.read(4)
        self.assertEqual(type(b), ConstBitStream)

    def testConstBitStreamProperties(self):
        a = ConstBitStream('0x123123')
        try:
            a.hex = '0x234'
            self.assertTrue(False)
        except AttributeError:
            pass
        try:
            a.oct = '0o234'
            self.assertTrue(False)
        except AttributeError:
            pass
        try:
            a.bin = '0b101'
            self.assertTrue(False)
        except AttributeError:
            pass
        try:
            a.ue = 3453
            self.assertTrue(False)
        except AttributeError:
            pass
        try:
            a.se = -123
            self.assertTrue(False)
        except AttributeError:
            pass
        try:
            a.int = 432
            self.assertTrue(False)
        except AttributeError:
            pass
        try:
            a.uint = 4412
            self.assertTrue(False)
        except AttributeError:
            pass
        try:
            a.intle = 123
            self.assertTrue(False)
        except AttributeError:
            pass
        try:
            a.uintle = 4412
            self.assertTrue(False)
        except AttributeError:
            pass
        try:
            a.intbe = 123
            self.assertTrue(False)
        except AttributeError:
            pass
        try:
            a.uintbe = 4412
            self.assertTrue(False)
        except AttributeError:
            pass
        try:
            a.intne = 123
            self.assertTrue(False)
        except AttributeError:
            pass
        try:
            a.uintne = 4412
            self.assertTrue(False)
        except AttributeError:
            pass
        try:
            a.bytes = b'hello'
            self.assertTrue(False)
        except AttributeError:
            pass

    def testConstBitStreamMisc(self):
        a = ConstBitStream('0xf')
        b = a
        a += '0xe'
        self.assertEqual(b, '0xf')
        self.assertEqual(a, '0xfe')
        c = BitStream(a)
        self.assertEqual(a, c)
        a = ConstBitStream('0b1')
        a._append(a)
        self.assertEqual(a, '0b11')
        self.assertEqual(type(a), ConstBitStream)
        a._prepend(a)
        self.assertEqual(a, '0b1111')
        self.assertEqual(type(a), ConstBitStream)

    def testConstBitStreamHashibility(self):
        a = ConstBitStream('0x1')
        b = ConstBitStream('0x2')
        c = ConstBitStream('0x1')
        c.pos = 3
        s = set((a, b, c))
        self.assertEqual(len(s), 2)
        self.assertEqual(hash(a), hash(c))

    def testConstBitStreamCopy(self):
        a = ConstBitStream('0xabc')
        a.pos = 11
        b = copy.copy(a)
        b.pos = 4
        self.assertEqual(id(a._datastore), id(b._datastore))
        self.assertEqual(a.pos, 11)
        self.assertEqual(b.pos, 4)

    def testPython26stuff(self):
        s = BitStream('0xff')
        self.assertTrue(isinstance(s.tobytes(), bytes))
        self.assertTrue(isinstance(s.bytes, bytes))

    def testReadFromBits(self):
        a = ConstBitStream('0xaabbccdd')
        b = a.read(8)
        self.assertEqual(b, '0xaa')
        self.assertEqual(a[0:8], '0xaa')
        self.assertEqual(a[-1], True)
        a.pos = 0
        self.assertEqual(a.read(4).uint, 10)


class Set(unittest.TestCase):
    def testSet(self):
        a = BitStream(length=16)
        a.set(True, 0)
        self.assertEqual(a, '0b10000000 00000000')
        a.set(1, 15)
        self.assertEqual(a, '0b10000000 00000001')
        b = a[4:12]
        b.set(True, 1)
        self.assertEqual(b, '0b01000000')
        b.set(True, -1)
        self.assertEqual(b, '0b01000001')
        b.set(1, -8)
        self.assertEqual(b, '0b11000001')
        self.assertRaises(IndexError, b.set, True, -9)
        self.assertRaises(IndexError, b.set, True, 8)

    def testSetNegativeIndex(self):
        a = BitStream(10)
        a.set(1, -1)
        self.assertEqual(a.bin, '0000000001')
        a.set(1, [-1, -10])
        self.assertEqual(a.bin, '1000000001')
        self.assertRaises(IndexError, a.set, 1, [-11])

    def testFileBasedSetUnset(self):
        a = BitStream(filename='test.m1v')
        a.set(True, (0, 1, 2, 3, 4))
        self.assertEqual(a[0:32], '0xf80001b3')
        a = BitStream(filename='test.m1v')
        a.set(False, (28, 29, 30, 31))
        self.assertTrue(a.startswith('0x000001b0'))

    def testSetList(self):
        a = BitStream(length=18)
        a.set(True, range(18))
        self.assertEqual(a.int, -1)
        a.set(False, range(18))
        self.assertEqual(a.int, 0)

    def testUnset(self):
        a = BitStream(length=16, int=-1)
        a.set(False, 0)
        self.assertEqual(~a, '0b10000000 00000000')
        a.set(0, 15)
        self.assertEqual(~a, '0b10000000 00000001')
        b = a[4:12]
        b.set(False, 1)
        self.assertEqual(~b, '0b01000000')
        b.set(False, -1)
        self.assertEqual(~b, '0b01000001')
        b.set(False, -8)
        self.assertEqual(~b, '0b11000001')
        self.assertRaises(IndexError, b.set, False, -9)
        self.assertRaises(IndexError, b.set, False, 8)

    def testSetWholeBitStream(self):
        a = BitStream(14)
        a.set(1)
        self.assertTrue(a.all(1))
        a.set(0)
        self.assertTrue(a.all(0))


class Invert(unittest.TestCase):
    def testInvertBits(self):
        a = BitStream('0b111000')
        a.invert(range(a.len))
        self.assertEqual(a, '0b000111')
        a.invert([0, 1, -1])
        self.assertEqual(a, '0b110110')

    def testInvertWholeBitStream(self):
        a = BitStream('0b11011')
        a.invert()
        self.assertEqual(a, '0b00100')

    def testInvertSingleBit(self):
        a = BitStream('0b000001')
        a.invert(0)
        self.assertEqual(a.bin, '100001')
        a.invert(-1)
        self.assertEqual(a.bin, '100000')

    def testInvertErrors(self):
        a = BitStream(10)
        self.assertRaises(IndexError, a.invert, 10)
        self.assertRaises(IndexError, a.invert, -11)
        self.assertRaises(IndexError, a.invert, [1, 2, 10])


    #######################

    def testIor(self):
        a = BitStream('0b1101001')
        a |= '0b1110000'
        self.assertEqual(a, '0b1111001')
        b = a[2:]
        c = a[1:-1]
        b |= c
        self.assertEqual(c, '0b11100')
        self.assertEqual(b, '0b11101')

    def testIand(self):
        a = BitStream('0b0101010101000')
        a &= '0b1111110000000'
        self.assertEqual(a, '0b0101010000000')
        s = BitStream(filename='test.m1v', offset=26, length=24)
        s &= '0xff00ff'
        self.assertEqual(s, '0xcc0004')

    def testIxor(self):
        a = BitStream('0b11001100110011')
        a ^= '0b11111100000010'
        self.assertEqual(a, '0b00110000110001')

    def testLogicalInplaceErrors(self):
        a = BitStream(4)
        self.assertRaises(ValueError, a.__ior__, '0b111')
        self.assertRaises(ValueError, a.__iand__, '0b111')
        self.assertRaises(ValueError, a.__ixor__, '0b111')


class AllAndAny(unittest.TestCase):
    def testAll(self):
        a = BitStream('0b0111')
        self.assertTrue(a.all(True, (1, 3)))
        self.assertFalse(a.all(True, (0, 1, 2)))
        self.assertTrue(a.all(True, [-1]))
        self.assertFalse(a.all(True, [0]))

    def testFileBasedAll(self):
        a = BitStream(filename='test.m1v')
        self.assertTrue(a.all(True, [31]))
        a = BitStream(filename='test.m1v')
        self.assertTrue(a.all(False, (0, 1, 2, 3, 4)))

    def testFileBasedAny(self):
        a = BitStream(filename='test.m1v')
        self.assertTrue(a.any(True, (31, 12)))
        a = BitStream(filename='test.m1v')
        self.assertTrue(a.any(False, (0, 1, 2, 3, 4)))

    def testAny(self):
        a = BitStream('0b10011011')
        self.assertTrue(a.any(True, (1, 2, 3, 5)))
        self.assertFalse(a.any(True, (1, 2, 5)))
        self.assertTrue(a.any(True, (-1,)))
        self.assertFalse(a.any(True, (1,)))

    def testAllFalse(self):
        a = BitStream('0b0010011101')
        self.assertTrue(a.all(False, (0, 1, 3, 4)))
        self.assertFalse(a.all(False, (0, 1, 2, 3, 4)))

    def testAnyFalse(self):
        a = BitStream('0b01001110110111111111111111111')
        self.assertTrue(a.any(False, (4, 5, 6, 2)))
        self.assertFalse(a.any(False, (1, 15, 20)))

    def testAnyEmptyBitstring(self):
        a = ConstBitStream()
        self.assertFalse(a.any(True))
        self.assertFalse(a.any(False))

    def testAllEmptyBitStream(self):
        a = ConstBitStream()
        self.assertTrue(a.all(True))
        self.assertTrue(a.all(False))

    def testAnyWholeBitstring(self):
        a = ConstBitStream('0xfff')
        self.assertTrue(a.any(True))
        self.assertFalse(a.any(False))

    def testAllWholeBitstring(self):
        a = ConstBitStream('0xfff')
        self.assertTrue(a.all(True))
        self.assertFalse(a.all(False))

    def testErrors(self):
        a = BitStream('0xf')
        self.assertRaises(IndexError, a.all, True, [5])
        self.assertRaises(IndexError, a.all, True, [-5])
        self.assertRaises(IndexError, a.any, True, [5])
        self.assertRaises(IndexError, a.any, True, [-5])

    ###################

    def testFloatInitialisation(self):
        for f in (0.0000001, -1.0, 1.0, 0.2, -3.1415265, 1.331e32):
            a = BitStream(float=f, length=64)
            a.pos = 6
            self.assertEqual(a.float, f)
            a = BitStream('float:64=%s' % str(f))
            a.pos = 6
            self.assertEqual(a.float, f)
            a = BitStream('floatbe:64=%s' % str(f))
            a.pos = 6
            self.assertEqual(a.floatbe, f)
            a = BitStream('floatle:64=%s' % str(f))
            a.pos = 6
            self.assertEqual(a.floatle, f)
            a = BitStream('floatne:64=%s' % str(f))
            a.pos = 6
            self.assertEqual(a.floatne, f)
            b = BitStream(float=f, length=32)
            b.pos = 6
            self.assertAlmostEqual(b.float / f, 1.0)
            b = BitStream('float:32=%s' % str(f))
            b.pos = 6
            self.assertAlmostEqual(b.float / f, 1.0)
            b = BitStream('floatbe:32=%s' % str(f))
            b.pos = 6
            self.assertAlmostEqual(b.floatbe / f, 1.0)
            b = BitStream('floatle:32=%s' % str(f))
            b.pos = 6
            self.assertAlmostEqual(b.floatle / f, 1.0)
            b = BitStream('floatne:32=%s' % str(f))
            b.pos = 6
            self.assertAlmostEqual(b.floatne / f, 1.0)
        a = BitStream('0x12345678')
        a.pos = 6
        a.float = 23
        self.assertEqual(a.float, 23.0)

    def testFloatInitStrings(self):
        for s in ('5', '+0.0001', '-1e101', '4.', '.2', '-.65', '43.21E+32'):
            a = BitStream('float:64=%s' % s)
            self.assertEqual(a.float, float(s))

    def testFloatPacking(self):
        a = pack('>d', 0.01)
        self.assertEqual(a.float, 0.01)
        self.assertEqual(a.floatbe, 0.01)
        a.byteswap()
        self.assertEqual(a.floatle, 0.01)
        b = pack('>f', 1e10)
        self.assertAlmostEqual(b.float / 1e10, 1.0)
        c = pack('<f', 10.3)
        self.assertAlmostEqual(c.floatle / 10.3, 1.0)
        d = pack('>5d', 10.0, 5.0, 2.5, 1.25, 0.1)
        self.assertEqual(d.unpack('>5d'), [10.0, 5.0, 2.5, 1.25, 0.1])

    def testFloatReading(self):
        a = BitStream('floatle:64=12, floatbe:64=-0.01, floatne:64=3e33')
        x, y, z = a.readlist('floatle:64, floatbe:64, floatne:64')
        self.assertEqual(x, 12.0)
        self.assertEqual(y, -0.01)
        self.assertEqual(z, 3e33)
        a = BitStream('floatle:32=12, floatbe:32=-0.01, floatne:32=3e33')
        x, y, z = a.readlist('floatle:32, floatbe:32, floatne:32')
        self.assertAlmostEqual(x / 12.0, 1.0)
        self.assertAlmostEqual(y / -0.01, 1.0)
        self.assertAlmostEqual(z / 3e33, 1.0)
        a = BitStream('0b11, floatle:64=12, 0xfffff')
        a.pos = 2
        self.assertEqual(a.read('floatle:64'), 12.0)
        b = BitStream(floatle=20, length=32)
        b.floatle = 10.0
        b = [0] + b
        self.assertEqual(b[1:].floatle, 10.0)

    def testNonAlignedFloatReading(self):
        s = BitStream('0b1, float:32 = 10.0')
        x, y = s.readlist('1, float:32')
        self.assertEqual(y, 10.0)
        s[1:] = 'floatle:32=20.0'
        x, y = s.unpack('1, floatle:32')
        self.assertEqual(y, 20.0)

    def testFloatErrors(self):
        a = BitStream('0x3')
        self.assertRaises(bitstring.InterpretError, a._getfloat)
        self.assertRaises(bitstring.CreationError, a._setfloat, -0.2)
        for l in (8, 10, 12, 16, 30, 128, 200):
            self.assertRaises(ValueError, BitStream, float=1.0, length=l)
        self.assertRaises(bitstring.CreationError, BitStream, floatle=0.3, length=0)
        self.assertRaises(bitstring.CreationError, BitStream, floatle=0.3, length=1)
        self.assertRaises(bitstring.CreationError, BitStream, float=2)
        self.assertRaises(bitstring.InterpretError, a.read, 'floatle:2')

    def testReadErrorChangesPos(self):
        a = BitStream('0x123123')
        try:
            a.read('10, 5')
        except ValueError:
            pass
        self.assertEqual(a.pos, 0)

    def testRor(self):
        a = BitStream('0b11001')
        a.ror(0)
        self.assertEqual(a, '0b11001')
        a.ror(1)
        self.assertEqual(a, '0b11100')
        a.ror(5)
        self.assertEqual(a, '0b11100')
        a.ror(101)
        self.assertEqual(a, '0b01110')
        a = BitStream('0b1')
        a.ror(1000000)
        self.assertEqual(a, '0b1')

    def testRorErrors(self):
        a = BitStream()
        self.assertRaises(bitstring.Error, a.ror, 0)
        a += '0b001'
        self.assertRaises(ValueError, a.ror, -1)

    def testRol(self):
        a = BitStream('0b11001')
        a.rol(0)
        self.assertEqual(a, '0b11001')
        a.rol(1)
        self.assertEqual(a, '0b10011')
        a.rol(5)
        self.assertEqual(a, '0b10011')
        a.rol(101)
        self.assertEqual(a, '0b00111')
        a = BitStream('0b1')
        a.rol(1000000)
        self.assertEqual(a, '0b1')

    def testRolFromFile(self):
        a = BitStream(filename='test.m1v')
        l = a.len
        a.rol(1)
        self.assertTrue(a.startswith('0x000003'))
        self.assertEqual(a.len, l)
        self.assertTrue(a.endswith('0x0036e'))

    def testRorFromFile(self):
        a = BitStream(filename='test.m1v')
        l = a.len
        a.ror(1)
        self.assertTrue(a.startswith('0x800000'))
        self.assertEqual(a.len, l)
        self.assertTrue(a.endswith('0x000db'))

    def testRolErrors(self):
        a = BitStream()
        self.assertRaises(bitstring.Error, a.rol, 0)
        a += '0b001'
        self.assertRaises(ValueError, a.rol, -1)

    def testBytesToken(self):
        a = BitStream('0x010203')
        b = a.read('bytes:1')
        self.assertTrue(isinstance(b, bytes))
        self.assertEqual(b, b'\x01')
        x, y, z = a.unpack('4, bytes:2, uint')
        self.assertEqual(x, 0)
        self.assertEqual(y, b'\x10\x20')
        self.assertEqual(z, 3)
        s = pack('bytes:4', b'abcd')
        self.assertEqual(s.bytes, b'abcd')

    def testBytesTokenMoreThoroughly(self):
        a = BitStream('0x0123456789abcdef')
        a.pos += 16
        self.assertEqual(a.read('bytes:1'), b'\x45')
        self.assertEqual(a.read('bytes:3'), b'\x67\x89\xab')
        x, y, z = a.unpack('bits:28, bytes, bits:12')
        self.assertEqual(y, b'\x78\x9a\xbc')

    def testDedicatedReadFunctions(self):
        a = BitStream('0b11, uint:43=98798798172, 0b11111')
        x = a._readuint(43, 2)
        self.assertEqual(x, 98798798172)
        self.assertEqual(a.pos, 0)
        x = a._readint(43, 2)
        self.assertEqual(x, 98798798172)
        self.assertEqual(a.pos, 0)

        a = BitStream('0b11, uintbe:48=98798798172, 0b11111')
        x = a._readuintbe(48, 2)
        self.assertEqual(x, 98798798172)
        self.assertEqual(a.pos, 0)
        x = a._readintbe(48, 2)
        self.assertEqual(x, 98798798172)
        self.assertEqual(a.pos, 0)

        a = BitStream('0b111, uintle:40=123516, 0b111')
        self.assertEqual(a._readuintle(40, 3), 123516)
        b = BitStream('0xff, uintle:800=999, 0xffff')
        self.assertEqual(b._readuintle(800, 8), 999)

        a = BitStream('0b111, intle:48=999999999, 0b111111111111')
        self.assertEqual(a._readintle(48, 3), 999999999)
        b = BitStream('0xff, intle:200=918019283740918263512351235, 0xfffffff')
        self.assertEqual(b._readintle(200, 8), 918019283740918263512351235)

        a = BitStream('0b111, floatbe:64=-5.32, 0xffffffff')
        self.assertEqual(a._readfloat(64, 3), -5.32)

        a = BitStream('0b111, floatle:64=9.9998, 0b111')
        self.assertEqual(a._readfloatle(64, 3), 9.9998)

    def testAutoInitWithInt(self):
        a = BitStream(0)
        self.assertFalse(a)
        a = BitStream(1)
        self.assertEqual(a, '0b0')
        a = BitStream(1007)
        self.assertEqual(a, BitStream(length=1007))
        self.assertRaises(bitstring.CreationError, BitStream, -1)

        a = 6 + ConstBitStream('0b1') + 3
        self.assertEqual(a, '0b0000001000')
        a += 1
        self.assertEqual(a, '0b00000010000')
        self.assertEqual(ConstBitStream(13), 13)

    def testReadingProblems(self):
        a = BitStream('0x000001')
        b = a.read('uint:24')
        self.assertEqual(b, 1)
        a.pos = 0
        self.assertRaises(bitstring.ReadError, a.read, 'bytes:4')

    def testAddVersesInPlaceAdd(self):
        a1 = ConstBitStream('0xabc')
        b1 = a1
        a1 += '0xdef'
        self.assertEqual(a1, '0xabcdef')
        self.assertEqual(b1, '0xabc')

        a2 = BitStream('0xabc')
        b2 = a2
        c2 = a2 + '0x0'
        a2 += '0xdef'
        self.assertEqual(a2, '0xabcdef')
        self.assertEqual(b2, '0xabcdef')
        self.assertEqual(c2, '0xabc0')

    def testAndVersesInPlaceAnd(self):
        a1 = ConstBitStream('0xabc')
        b1 = a1
        a1 &= '0xf0f'
        self.assertEqual(a1, '0xa0c')
        self.assertEqual(b1, '0xabc')

        a2 = BitStream('0xabc')
        b2 = a2
        c2 = a2 & '0x00f'
        a2 &= '0xf0f'
        self.assertEqual(a2, '0xa0c')
        self.assertEqual(b2, '0xa0c')
        self.assertEqual(c2, '0x00c')

    def testOrVersesInPlaceOr(self):
        a1 = ConstBitStream('0xabc')
        b1 = a1
        a1 |= '0xf0f'
        self.assertEqual(a1, '0xfbf')
        self.assertEqual(b1, '0xabc')

        a2 = BitStream('0xabc')
        b2 = a2
        c2 = a2 | '0x00f'
        a2 |= '0xf0f'
        self.assertEqual(a2, '0xfbf')
        self.assertEqual(b2, '0xfbf')
        self.assertEqual(c2, '0xabf')

    def testXorVersesInPlaceXor(self):
        a1 = ConstBitStream('0xabc')
        b1 = a1
        a1 ^= '0xf0f'
        self.assertEqual(a1, '0x5b3')
        self.assertEqual(b1, '0xabc')

        a2 = BitStream('0xabc')
        b2 = a2
        c2 = a2 ^ '0x00f'
        a2 ^= '0xf0f'
        self.assertEqual(a2, '0x5b3')
        self.assertEqual(b2, '0x5b3')
        self.assertEqual(c2, '0xab3')

    def testMulVersesInPlaceMul(self):
        a1 = ConstBitStream('0xabc')
        b1 = a1
        a1 *= 3
        self.assertEqual(a1, '0xabcabcabc')
        self.assertEqual(b1, '0xabc')

        a2 = BitStream('0xabc')
        b2 = a2
        c2 = a2 * 2
        a2 *= 3
        self.assertEqual(a2, '0xabcabcabc')
        self.assertEqual(b2, '0xabcabcabc')
        self.assertEqual(c2, '0xabcabc')

    def testLshiftVersesInPlaceLshift(self):
        a1 = ConstBitStream('0xabc')
        b1 = a1
        a1 <<= 4
        self.assertEqual(a1, '0xbc0')
        self.assertEqual(b1, '0xabc')

        a2 = BitStream('0xabc')
        b2 = a2
        c2 = a2 << 8
        a2 <<= 4
        self.assertEqual(a2, '0xbc0')
        self.assertEqual(b2, '0xbc0')
        self.assertEqual(c2, '0xc00')

    def testRshiftVersesInPlaceRshift(self):
        a1 = ConstBitStream('0xabc')
        b1 = a1
        a1 >>= 4
        self.assertEqual(a1, '0x0ab')
        self.assertEqual(b1, '0xabc')

        a2 = BitStream('0xabc')
        b2 = a2
        c2 = a2 >> 8
        a2 >>= 4
        self.assertEqual(a2, '0x0ab')
        self.assertEqual(b2, '0x0ab')
        self.assertEqual(c2, '0x00a')

    def testAutoFromBool(self):
        a = ConstBitStream() + True + False + True
        self.assertEqual(a, '0b00')
        #    self.assertEqual(a, '0b101')
        #    b = ConstBitStream(False)
        #    self.assertEqual(b, '0b0')
        #    c = ConstBitStream(True)
        #    self.assertEqual(c, '0b1')
        #    self.assertEqual(b, False)
        #    self.assertEqual(c, True)
        #    self.assertEqual(b & True, False)


class Bugs(unittest.TestCase):
    def testBugInReplace(self):
        s = BitStream('0x00112233')
        l = list(s.split('0x22', start=8, bytealigned=True))
        self.assertEqual(l, ['0x11', '0x2233'])
        s = BitStream('0x00112233')
        s.replace('0x22', '0xffff', start=8, bytealigned=True)
        self.assertEqual(s, '0x0011ffff33')
        s = BitStream('0x0123412341234')
        s.replace('0x23', '0xf', start=9, bytealigned=True)
        self.assertEqual(s, '0x012341f41f4')

    def testTruncateStartBug(self):
        a = BitStream('0b000000111')[2:]
        a._truncatestart(6)
        self.assertEqual(a, '0b1')

    def testNullBits(self):
        s = ConstBitStream(bin='')
        t = ConstBitStream(oct='')
        u = ConstBitStream(hex='')
        v = ConstBitStream(bytes=b'')
        self.assertFalse(s)
        self.assertFalse(t)
        self.assertFalse(u)
        self.assertFalse(v)

    def testMultiplicativeFactorsCreation(self):
        s = BitStream('1*0b1')
        self.assertEqual(s, '0b1')
        s = BitStream('4*0xc')
        self.assertEqual(s, '0xcccc')
        s = BitStream('0b1, 0*0b0')
        self.assertEqual(s, '0b1')
        s = BitStream('0b1, 3*uint:8=34, 2*0o755')
        self.assertEqual(s, '0b1, uint:8=34, uint:8=34, uint:8=34, 0o755755')
        s = BitStream('0*0b1001010')
        self.assertFalse(s)

    def testMultiplicativeFactorsReading(self):
        s = BitStream('0xc') * 5
        a, b, c, d, e = s.readlist('5*4')
        self.assertTrue(a == b == c == d == e == 12)
        s = ConstBitStream('2*0b101, 4*uint:7=3')
        a, b, c, d, e = s.readlist('2*bin:3, 3*uint:7')
        self.assertTrue(a == b == '101')
        self.assertTrue(c == d == e == 3)

    def testMultiplicativeFactorsPacking(self):
        s = pack('3*bin', '1', '001', '101')
        self.assertEqual(s, '0b1001101')
        s = pack('hex, 2*se=-56, 3*uint:37', '34', 1, 2, 3)
        a, b, c, d, e, f = s.unpack('hex:8, 2*se, 3*uint:37')
        self.assertEqual(a, '34')
        self.assertEqual(b, -56)
        self.assertEqual(c, -56)
        self.assertEqual((d, e, f), (1, 2, 3))
        # This isn't allowed yet. See comment in tokenparser.
        #s = pack('fluffy*uint:8', *range(3), fluffy=3)
        #a, b, c = s.readlist('2*uint:8, 1*uint:8, 0*uint:8')
        #self.assertEqual((a, b, c), (0, 1, 2))

    def testMultiplicativeFactorsUnpacking(self):
        s = ConstBitStream('0b10111')
        a, b, c, d = s.unpack('3*bool, bin')
        self.assertEqual((a, b, c), (True, False, True))
        self.assertEqual(d, '11')


    def testPackingDefaultIntWithKeyword(self):
        s = pack('12', 100)
        self.assertEqual(s.unpack('12')[0], 100)
        s = pack('oh_no_not_the_eyes=33', oh_no_not_the_eyes=17)
        self.assertEqual(s.uint, 33)
        self.assertEqual(s.len, 17)

    def testInitFromIterable(self):
        self.assertTrue(isinstance(range(10), collections.Iterable))
        s = ConstBitStream(range(12))
        self.assertEqual(s, '0x7ff')

    def testFunctionNegativeIndices(self):
        # insert
        s = BitStream('0b0111')
        s.insert('0b0', -1)
        self.assertEqual(s, '0b01101')
        self.assertRaises(ValueError, s.insert, '0b0', -1000)

        # reverse
        s.reverse(-2)
        self.assertEqual(s, '0b01110')
        t = BitStream('0x778899abcdef')
        t.reverse(-12, -4)
        self.assertEqual(t, '0x778899abc7bf')

        # reversebytes
        t.byteswap(0, -40, -16)
        self.assertEqual(t, '0x77ab9988c7bf')

        # overwrite
        t.overwrite('0x666', -20)
        self.assertEqual(t, '0x77ab998666bf')

        # find
        found = t.find('0x998', bytealigned=True, start=-31)
        self.assertFalse(found)
        found = t.find('0x998', bytealigned=True, start=-32)
        self.assertTrue(found)
        self.assertEqual(t.pos, 16)
        t.pos = 0
        found = t.find('0x988', bytealigned=True, end=-21)
        self.assertFalse(found)
        found = t.find('0x998', bytealigned=True, end=-20)
        self.assertTrue(found)
        self.assertEqual(t.pos, 16)

        #findall
        s = BitStream('0x1234151f')
        l = list(s.findall('0x1', bytealigned=True, start=-15))
        self.assertEqual(l, [24])
        l = list(s.findall('0x1', bytealigned=True, start=-16))
        self.assertEqual(l, [16, 24])
        l = list(s.findall('0x1', bytealigned=True, end=-5))
        self.assertEqual(l, [0, 16])
        l = list(s.findall('0x1', bytealigned=True, end=-4))
        self.assertEqual(l, [0, 16, 24])

        # rfind
        found = s.rfind('0x1f', end=-1)
        self.assertFalse(found)
        found = s.rfind('0x12', start=-31)
        self.assertFalse(found)

        # cut
        s = BitStream('0x12345')
        l = list(s.cut(4, start=-12, end=-4))
        self.assertEqual(l, ['0x3', '0x4'])

        # split
        s = BitStream('0xfe0012fe1200fe')
        l = list(s.split('0xfe', bytealigned=True, end=-1))
        self.assertEqual(l, ['', '0xfe0012', '0xfe1200f, 0b111'])
        l = list(s.split('0xfe', bytealigned=True, start=-8))
        self.assertEqual(l, ['', '0xfe'])

        # startswith
        self.assertTrue(s.startswith('0x00f', start=-16))
        self.assertTrue(s.startswith('0xfe00', end=-40))
        self.assertFalse(s.startswith('0xfe00', end=-41))

        # endswith
        self.assertTrue(s.endswith('0x00fe', start=-16))
        self.assertFalse(s.endswith('0x00fe', start=-15))
        self.assertFalse(s.endswith('0x00fe', end=-1))
        self.assertTrue(s.endswith('0x00f', end=-4))

        # replace
        s.replace('0xfe', '', end=-1)
        self.assertEqual(s, '0x00121200fe')
        s.replace('0x00', '', start=-24)
        self.assertEqual(s, '0x001212fe')

    def testRotateStartAndEnd(self):
        a = BitStream('0b110100001')
        a.rol(1, 3, 6)
        self.assertEqual(a, '0b110001001')
        a.ror(1, start=-4)
        self.assertEqual(a, '0b110001100')
        a.rol(202, end=-5)
        self.assertEqual(a, '0b001101100')
        a.ror(3, end=4)
        self.assertEqual(a, '0b011001100')
        self.assertRaises(ValueError, a.rol, 5, start=-4, end=-6)

    def testByteSwapInt(self):
        s = pack('5*uintle:16', *range(10, 15))
        self.assertEqual(list(range(10, 15)), s.unpack('5*uintle:16'))
        swaps = s.byteswap(2)
        self.assertEqual(list(range(10, 15)), s.unpack('5*uintbe:16'))
        self.assertEqual(swaps, 5)
        s = BitStream('0xf234567f')
        swaps = s.byteswap(1, start=4)
        self.assertEqual(swaps, 3)
        self.assertEqual(s, '0xf234567f')
        s.byteswap(2, start=4)
        self.assertEqual(s, '0xf452367f')
        s.byteswap(2, start=4, end=-4)
        self.assertEqual(s, '0xf234567f')
        s.byteswap(3)
        self.assertEqual(s, '0x5634f27f')
        s.byteswap(2, repeat=False)
        self.assertEqual(s, '0x3456f27f')
        swaps = s.byteswap(5)
        self.assertEqual(swaps, 0)
        swaps = s.byteswap(4, repeat=False)
        self.assertEqual(swaps, 1)
        self.assertEqual(s, '0x7ff25634')

    def testByteSwapPackCode(self):
        s = BitStream('0x0011223344556677')
        swaps = s.byteswap('b')
        self.assertEqual(s, '0x0011223344556677')
        self.assertEqual(swaps, 8)
        swaps = s.byteswap('>3h', repeat=False)
        self.assertEqual(s, '0x1100332255446677')
        self.assertEqual(swaps, 1)

    def testByteSwapIterable(self):
        s = BitStream('0x0011223344556677')
        swaps = s.byteswap(range(1, 4), repeat=False)
        self.assertEqual(swaps, 1)
        self.assertEqual(s, '0x0022115544336677')
        swaps = s.byteswap([2], start=8)
        self.assertEqual(s, '0x0011224455663377')
        self.assertEqual(3, swaps)
        swaps = s.byteswap([2, 3], start=4)
        self.assertEqual(swaps, 1)
        self.assertEqual(s, '0x0120156452463377')

    def testByteSwapErrors(self):
        s = BitStream('0x0011223344556677')
        self.assertRaises(ValueError, s.byteswap, 'z')
        self.assertRaises(ValueError, s.byteswap, -1)
        self.assertRaises(ValueError, s.byteswap, [-1])
        self.assertRaises(ValueError, s.byteswap, [1, 'e'])
        self.assertRaises(ValueError, s.byteswap, '!h')
        self.assertRaises(ValueError, s.byteswap, 2, start=-1000)
        self.assertRaises(TypeError, s.byteswap, 5.4)

    def testByteSwapFromFile(self):
        s = BitStream(filename='smalltestfile')
        swaps = s.byteswap('2bh')
        self.assertEqual(s, '0x0123674589abefcd')
        self.assertEqual(swaps, 2)

    def testBracketExpander(self):
        be = bitstring.expand_brackets
        self.assertEqual(be('hello'), 'hello')
        self.assertEqual(be('(hello)'), 'hello')
        self.assertEqual(be('1*(hello)'), 'hello')
        self.assertEqual(be('2*(hello)'), 'hello,hello')
        self.assertEqual(be('1*(a, b)'), 'a,b')
        self.assertEqual(be('2*(a, b)'), 'a,b,a,b')
        self.assertEqual(be('2*(a), 3*(b)'), 'a,a,b,b,b')
        self.assertEqual(be('2*(a, b, 3*(c, d), e)'), 'a,b,c,d,c,d,c,d,e,a,b,c,d,c,d,c,d,e')

    def testBracketTokens(self):
        s = BitStream('3*(0x0, 0b1)')
        self.assertEqual(s, '0x0, 0b1, 0x0, 0b1, 0x0, 0b1')
        s = pack('2*(uint:12, 3*(7, 6))', *range(3, 17))
        a = s.unpack('12, 7, 6, 7, 6, 7, 6, 12, 7, 6, 7, 6, 7, 6')
        self.assertEqual(a, list(range(3, 17)))
        b = s.unpack('2*(12,3*(7,6))')
        self.assertEqual(a, b)

    def testPackCodeDicts(self):
        self.assertEqual(sorted(bitstring.REPLACEMENTS_BE.keys()),
                         sorted(bitstring.REPLACEMENTS_LE.keys()))
        self.assertEqual(sorted(bitstring.REPLACEMENTS_BE.keys()),
                         sorted(bitstring.PACK_CODE_SIZE.keys()))
        for key in bitstring.PACK_CODE_SIZE:
            be = pack(bitstring.REPLACEMENTS_BE[key], 0)
            le = pack(bitstring.REPLACEMENTS_LE[key], 0)
            self.assertEqual(be.len, bitstring.PACK_CODE_SIZE[key] * 8)
            self.assertEqual(le.len, be.len)

            # These tests don't compile for Python 3, so they're commented out to save me stress.
            #def testUnicode(self):
            #a = ConstBitStream(u'uint:12=34')
            #self.assertEqual(a.uint, 34)
            #a += u'0xfe'
            #self.assertEqual(a[12:], '0xfe')
            #a = BitStream('0x1122')
            #c = a.byteswap(u'h')
            #self.assertEqual(c, 1)
            #self.assertEqual(a, u'0x2211')

            #def testLongInt(self):
            #a = BitStream(4L)
            #self.assertEqual(a, '0b0000')
            #a[1:3] = -1L
            #self.assertEqual(a, '0b0110')
            #a[0] = 1L
            #self.assertEqual(a, '0b1110')
            #a *= 4L
            #self.assertEqual(a, '0xeeee')
            #c = a.byteswap(2L)
            #self.assertEqual(c, 1)
            #a = BitStream('0x11223344')
            #a.byteswap([1, 2L])
            #self.assertEqual(a, '0x11332244')
            #b = a*2L
            #self.assertEqual(b, '0x1133224411332244')
            #s = pack('uint:12', 46L)
            #self.assertEqual(s.uint, 46)


class UnpackWithDict(unittest.TestCase):
    def testLengthKeywords(self):
        a = ConstBitStream('2*13=100, 0b111')
        x, y, z = a.unpack('n, uint:m, bin:q', n=13, m=13, q=3)
        self.assertEqual(x, 100)
        self.assertEqual(y, 100)
        self.assertEqual(z, '111')

    def testLengthKeywordsWithStretch(self):
        a = ConstBitStream('0xff, 0b000, 0xf')
        x, y, z = a.unpack('hex:a, bin, hex:b', a=8, b=4)
        self.assertEqual(y, '000')

    def testUnusedKeyword(self):
        a = ConstBitStream('0b110')
        x, = a.unpack('bin:3', notused=33)
        self.assertEqual(x, '110')

    def testLengthKeywordErrors(self):
        a = pack('uint:p=33', p=12)
        self.assertRaises(ValueError, a.unpack, 'uint:p')
        self.assertRaises(ValueError, a.unpack, 'uint:p', p='a_string')


class ReadWithDict(unittest.TestCase):
    def testLengthKeywords(self):
        s = BitStream('0x0102')
        x, y = s.readlist('a, hex:b', a=8, b=4)
        self.assertEqual((x, y), (1, '0'))
        self.assertEqual(s.pos, 12)

    def testBytesKeywordProblem(self):
        s = BitStream('0x01')
        x, = s.unpack('bytes:a', a=1)
        self.assertEqual(x, b'\x01')

        s = BitStream('0x000ff00a')
        x, y, z = s.unpack('12, bytes:x, bits', x=2)
        self.assertEqual((x, y, z), (0, b'\xff\x00', '0xa'))



class PeekWithDict(unittest.TestCase):
    def testLengthKeywords(self):
        s = BitStream('0x0102')
        x, y = s.peeklist('a, hex:b', a=8, b=4)
        self.assertEqual((x, y), (1, '0'))
        self.assertEqual(s.pos, 0)

##class Miscellany(unittest.TestCase):
##
##    def testNumpyInt(self):
##        try:
##            import numpy
##            a = ConstBitStream(uint=numpy.uint8(5), length=3)
##            self.assertEqual(a.uint, 5)
##        except ImportError:
##            # Not to worry
##            pass

class BoolToken(unittest.TestCase):
    def testInterpretation(self):
        a = ConstBitStream('0b1')
        self.assertEqual(a.bool, True)
        self.assertEqual(a.read('bool'), True)
        self.assertEqual(a.unpack('bool')[0], True)
        b = ConstBitStream('0b0')
        self.assertEqual(b.bool, False)
        self.assertEqual(b.peek('bool'), False)
        self.assertEqual(b.unpack('bool')[0], False)

    def testPack(self):
        a = pack('bool=True')
        b = pack('bool=False')
        self.assertEqual(a.bool, True)
        self.assertEqual(b.bool, False)
        c = pack('4*bool', False, True, 'False', 'True')
        self.assertEqual(c, '0b0101')

    def testAssignment(self):
        a = BitStream()
        a.bool = True
        self.assertEqual(a.bool, True)
        a.hex = 'ee'
        a.bool = False
        self.assertEqual(a.bool, False)
        a.bool = 'False'
        self.assertEqual(a.bool, False)
        a.bool = 'True'
        self.assertEqual(a.bool, True)
        a.bool = 0
        self.assertEqual(a.bool, False)
        a.bool = 1
        self.assertEqual(a.bool, True)

    def testErrors(self):
        self.assertRaises(bitstring.CreationError, pack, 'bool', 'hello')
        self.assertRaises(bitstring.CreationError, pack, 'bool=true')
        self.assertRaises(bitstring.CreationError, pack, 'True')
        self.assertRaises(bitstring.CreationError, pack, 'bool', 2)
        a = BitStream('0b11')
        self.assertRaises(bitstring.InterpretError, a._getbool)
        b = BitStream()
        self.assertRaises(bitstring.InterpretError, a._getbool)
        self.assertRaises(bitstring.CreationError, a._setbool, 'false')

    def testLengthWithBoolRead(self):
        a = ConstBitStream('0xf')
        self.assertRaises(ValueError, a.read, 'bool:0')
        self.assertRaises(ValueError, a.read, 'bool:1')
        self.assertRaises(ValueError, a.read, 'bool:2')


class ReadWithIntegers(unittest.TestCase):
    def testReadInt(self):
        a = ConstBitStream('0xffeedd')
        b = a.read(8)
        self.assertEqual(b.hex, 'ff')
        self.assertEqual(a.pos, 8)
        b = a.peek(8)
        self.assertEqual(b.hex, 'ee')
        self.assertEqual(a.pos, 8)
        b = a.peek(1)
        self.assertEqual(b, '0b1')
        b = a.read(1)
        self.assertEqual(b, '0b1')

    def testReadIntList(self):
        a = ConstBitStream('0xab, 0b110')
        b, c = a.readlist([8, 3])
        self.assertEqual(b.hex, 'ab')
        self.assertEqual(c.bin, '110')


class FileReadingStrategy(unittest.TestCase):
    def testBitStreamIsAlwaysRead(self):
        a = BitStream(filename='smalltestfile')
        self.assertTrue(isinstance(a._datastore, bitstring.ByteStore))
        f = open('smalltestfile', 'rb')
        b = BitStream(f)
        self.assertTrue(isinstance(b._datastore, bitstring.ByteStore))

    def testBitsIsNeverRead(self):
        a = ConstBitStream(filename='smalltestfile')
        self.assertTrue(isinstance(a._datastore._rawarray, bitstring.MmapByteArray))
        f = open('smalltestfile', 'rb')
        b = ConstBitStream(f)
        self.assertTrue(isinstance(b._datastore._rawarray, bitstring.MmapByteArray))


class Count(unittest.TestCase):
    def testCount(self):
        a = ConstBitStream('0xf0f')
        self.assertEqual(a.count(True), 8)
        self.assertEqual(a.count(False), 4)

        b = BitStream()
        self.assertEqual(b.count(True), 0)
        self.assertEqual(b.count(False), 0)

    def testCountWithOffsetData(self):
        a = ConstBitStream('0xff0120ff')
        b = a[1:-1]
        self.assertEqual(b.count(1), 16)
        self.assertEqual(b.count(0), 14)


class ZeroBitReads(unittest.TestCase):
    def testInteger(self):
        a = ConstBitStream('0x123456')
        self.assertRaises(bitstring.InterpretError, a.read, 'uint:0')
        self.assertRaises(bitstring.InterpretError, a.read, 'float:0')

#class EfficientBitsCopies(unittest.TestCase):
#
#    def testBitsCopy(self):
#        a = ConstBitStream('0xff')
#        b = ConstBitStream(a)
#        c = a[:]
#        d = copy.copy(a)
#        self.assertTrue(a._datastore is b._datastore)
#        self.assertTrue(a._datastore is c._datastore)
#        self.assertTrue(a._datastore is d._datastore)

class InitialiseFromBytes(unittest.TestCase):
    def testBytesBehaviour(self):
        a = ConstBitStream(b'uint:5=2')
        b = ConstBitStream(b'')
        c = ConstBitStream(bytes=b'uint:5=2')
        if b'' == '':
            # Python 2
            self.assertEqual(a, 'uint:5=2')
            self.assertFalse(b)
            self.assertEqual(c.bytes, b'uint:5=2')
        else:
            self.assertEqual(a.bytes, b'uint:5=2')
            self.assertFalse(b)
            self.assertEqual(c, b'uint:5=2')

    def testBytearrayBehaviour(self):
        a = ConstBitStream(bytearray(b'uint:5=2'))
        b = ConstBitStream(bytearray(4))
        c = ConstBitStream(bytes=bytearray(b'uint:5=2'))
        self.assertEqual(a.bytes, b'uint:5=2')
        self.assertEqual(b, '0x00000000')
        self.assertEqual(c.bytes, b'uint:5=2')


class CoverageCompletionTests(unittest.TestCase):
    def testUeReadError(self):
        s = ConstBitStream('0b000000001')
        self.assertRaises(bitstring.ReadError, s.read, 'ue')

    def testOverwriteWithSelf(self):
        s = BitStream('0b1101')
        s.overwrite(s)
        self.assertEqual(s, '0b1101')


class Subclassing(unittest.TestCase):

    def testIsInstance(self):
        class SubBits(BitStream): pass
        a = SubBits()
        self.assertTrue(isinstance(a, SubBits))

    def testClassType(self):
        class SubBits(BitStream): pass
        self.assertEqual(SubBits().__class__, SubBits)


class BytesProblems(unittest.TestCase):

    def testOffsetButNoLength(self):
        b = BitStream(bytes=b'\x00\xaa', offset=8)
        self.assertEqual(b.hex, 'aa')
        b = BitStream(bytes=b'\x00\xaa', offset=4)
        self.assertEqual(b.hex, '0aa')

    def testInvert(self):
        b = BitStream(bytes=b'\x00\xaa', offset=8, length=8)
        self.assertEqual(b.hex, 'aa')
        b.invert()
        self.assertEqual(b.hex, '55')

    def testPrepend(self):
        b = BitStream(bytes=b'\xaa\xbb', offset=8, length=4)
        self.assertEqual(b.hex, 'b')
        b.prepend('0xe')
        self.assertEqual(b.hex, 'eb')
        b = BitStream(bytes=b'\x00\xaa', offset=8, length=8)
        b.prepend('0xee')
        self.assertEqual(b.hex, 'eeaa')

    def testByteSwap(self):
        b = BitStream(bytes=b'\x01\x02\x03\x04', offset=8)
        b.byteswap()
        self.assertEqual(b, '0x040302')

    def testBinProperty(self):
        b = BitStream(bytes=b'\x00\xaa', offset=8, length=4)
        self.assertEqual(b.bin, '1010')