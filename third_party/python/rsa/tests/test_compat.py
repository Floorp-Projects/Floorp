# -*- coding: utf-8 -*-

import unittest2
import struct

from rsa._compat import is_bytes, byte

class Test_byte(unittest2.TestCase):
    def test_byte(self):
        for i in range(256):
            byt = byte(i)
            self.assertTrue(is_bytes(byt))
            self.assertEqual(ord(byt), i)

    def test_raises_StructError_on_overflow(self):
        self.assertRaises(struct.error, byte, 256)
        self.assertRaises(struct.error, byte, -1)
