#!/usr/bin/env python

import unittest
import sys
sys.path.insert(0, '..')
from bitstring import ByteStore, ConstByteStore, equal, offsetcopy


class OffsetCopy(unittest.TestCase):
    def testStraightCopy(self):
        s = ByteStore(bytearray([10, 5, 1]), 24, 0)
        t = offsetcopy(s, 0)
        self.assertEqual(t._rawarray, bytearray([10, 5, 1]))

    def testOffsetIncrease(self):
        s = ByteStore(bytearray([1, 1, 1]), 24, 0)
        t = offsetcopy(s, 4)
        self.assertEqual(t.bitlength, 24)
        self.assertEqual(t.offset, 4)
        self.assertEqual(t._rawarray, bytearray([0, 16, 16, 16]))


class Equals(unittest.TestCase):

    def testBothSingleByte(self):
        s = ByteStore(bytearray([128]), 3, 0)
        t = ByteStore(bytearray([64]), 3, 1)
        u = ByteStore(bytearray([32]), 3, 2)
        self.assertTrue(equal(s, t))
        self.assertTrue(equal(s, u))
        self.assertTrue(equal(u, t))

    def testOneSingleByte(self):
        s = ByteStore(bytearray([1, 0]), 2, 7)
        t = ByteStore(bytearray([64]), 2, 1)
        self.assertTrue(equal(s, t))
        self.assertTrue(equal(t, s))