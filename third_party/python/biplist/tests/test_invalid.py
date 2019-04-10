# -*- coding: utf-8 -*-

from biplist import *
from biplist import PlistTrailer
from collections import namedtuple
from math import pow
import os
from struct import pack
from test_utils import *
import unittest

def trailerToString(trailer):
    # Trailer is:
    # 6 padding bytes
    # 1 byte offsetSize
    # 1 byte objectRefSize
    # 8 bytes offsetCount (number of objects)
    # 8 bytes topObjectNumber
    # 8 bytes offsetTableOffset
    # = 32 byte trailer
    return pack('!xxxxxxBBQQQ', *trailer)

class TestInvalidPlistFile(unittest.TestCase):
    def setUp(self):
        pass
    def testEmptyFile(self):
        try:
            readPlist(data_path('empty_file.plist'))
            self.fail("Should not successfully read empty plist.")
        except NotBinaryPlistException as e:
            pass
        except InvalidPlistException as e:
            pass
    
    def testTooShort(self):
        try:
            readPlistFromString(b"bplist0")
            self.fail("Should not successfully read plist which is too short.")
        except InvalidPlistException as e:
            pass
    
    def testInvalid(self):
        try:
            readPlistFromString(b"bplist0-------------------------------------")
            self.fail("Should not successfully read invalid plist.")
        except InvalidPlistException as e:
            pass
    
    def testInvalidTemplate(self):
        # Test that our template plists for range tests are valid.
        trailer = PlistTrailer(1, 1, 1, 0, 9)
        contents = b''.join([b'bplist00', # header
                             b'\x08',     # bool false
                             b'\x08',     # object at offset 8
                             trailerToString(trailer)
                         ])
        result = readPlistFromString(contents)
        self.assertEqual(result, False)
        
        trailer = PlistTrailer(1, 1, 3, 0, 25)
        contents = b''.join([b'bplist00bybiplist1.0', # header
                             b'\xA2',     # array with two entries
                             b'\x01',     # object entry 1
                             b'\x02',     # object entry 2
                             b'\x09',     # boolean false
                             b'\x08',     # boolean true
                             b'\x14',     # offset at 20
                             b'\x17',     # offset at 23
                             b'\x18',     # offset at 24
                             trailerToString(trailer)
                         ])
        result = readPlistFromString(contents)
        self.assertEqual(result, [True, False])
    
    def testInvalidOffsetSize(self):
        # offset size can't be zero
        try:
            trailer = PlistTrailer(0, 1, 1, 0, 9)
            contents = b''.join([b'bplist00', # header
                                 b'\x08',     # bool false
                                 b'\x08',     # object at offset 8
                                 trailerToString(trailer)
                             ])
            readPlistFromString(contents)
            self.fail("Offset size can't be zero")
        except InvalidPlistException as e:
            pass
        
        # offset size can't be greater than 16
        try:
            trailer = PlistTrailer(17, 1, 1, 0, 9)
            contents = b''.join([b'bplist00', # header
                                 b'\x08',     # bool false
                                 b'\x08',     # object at offset 8
                                 trailerToString(trailer)
                             ])
            readPlistFromString(contents)
            self.fail("Offset size can't be greater than 16")
        except InvalidPlistException as e:
            pass
    
    def testInvalidOffsetOverflow(self):
        # The offsets can't overflow the number of bytes used to represent them.
        try:
            c = readPlist(data_path('invalid_object_offset_size.plist'))
            self.fail("Object offset size too small to reference all objects")
        except InvalidPlistException as e:
            pass
    
    def testInvalidObjectRefSize(self):
        # object reference size can't be zero
        try:
            trailer = PlistTrailer(1, 0, 1, 0, 9)
            contents = b''.join([b'bplist00', # header
                                 b'\x08',     # bool false
                                 b'\x08',     # object at offset 8
                                 trailerToString(trailer)
                             ])
            readPlistFromString(contents)
            self.fail("Object reference size can't be zero")
        except InvalidPlistException as e:
            pass
    
    def testInvalidObjectRefOverflow(self):
        try:
            readPlist(data_path('invalid_object_ref_size.plist'))
            self.fail("Object ref size too small to reference all objects in the object table")
        except InvalidPlistException as e:
            pass
    
    def testInvalidOffsestTableOffset(self):
        # offsetTableOffset too large, extending into trailer
        try:
            trailer = PlistTrailer(1, 1, 1, 0, 10)
            contents = b''.join([b'bplist00', # header
                                 b'\x08',     # bool false
                                 b'\x08',     # object at offset 8
                                 trailerToString(trailer)
                             ])
            readPlistFromString(contents)
            self.fail("Should not read plist when offsetTableOffset is too large")
        except InvalidPlistException as e:
            pass
        
        # offsetTableOffset too small, extending into header or objects
        try:
            trailer = PlistTrailer(1, 1, 1, 0, 8)
            contents = b''.join([b'bplist00', # header
                                 b'\x08',     # bool false
                                 b'\x08',     # object at offset 8
                                 trailerToString(trailer)
                             ])
            readPlistFromString(contents)
            self.fail("Should not read plist when offsetTableOffset is too small")
        except InvalidPlistException as e:
            pass
    
    def testInvalidTopObjectNumber(self):
        # topObjectNumber can't be greater than number of objects
        try:
            trailer = PlistTrailer(1, 1, 1, 1, 9)
            contents = b''.join([b'bplist00', # header
                                 b'\x08',     # bool false
                                 b'\x08',     # object at offset 8
                                 trailerToString(trailer)
                             ])
            readPlistFromString(contents)
            self.fail("Top object number should not be greater than number of objects")
        except InvalidPlistException as e:
            pass
        
if __name__ == '__main__':
    unittest.main()
