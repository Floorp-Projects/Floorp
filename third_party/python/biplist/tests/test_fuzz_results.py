# -*- coding: utf-8 -*-

from biplist import *
import os
from test_utils import *
import unittest

class TestFuzzResults(unittest.TestCase):
    def setUp(self):
        pass
    
    def testCurrentOffsetOutOfRange(self):
        try:
            readPlist(fuzz_data_path('list_index_out_of_range.plist'))
            self.fail("list index out of range, should fail")
        except NotBinaryPlistException as e:
            pass
        except InvalidPlistException as e:
            pass
    
    def testInvalidMarkerByteUnpack(self):
        try:
            readPlist(fuzz_data_path('no_marker_byte.plist'))
            self.fail("No marker byte at object offset")
        except NotBinaryPlistException as e:
            pass
        except InvalidPlistException as e:
            pass
    
    def testInvalidObjectOffset(self):
        try:
            readPlist(fuzz_data_path('invalid_object_offset.plist'))
            self.fail("Invalid object offset in offsets table")
        except NotBinaryPlistException as e:
            pass
        except InvalidPlistException as e:
            pass
    
    def testRecursiveObjectOffset(self):
        try:
            readPlist(fuzz_data_path('recursive_object_offset.plist'))
            self.fail("Recursive object found")
        except NotBinaryPlistException as e:
            pass
        except InvalidPlistException as e:
            pass
    
    def testExcessivelyLongAsciiString(self):
        try:
            readPlist(fuzz_data_path('ascii_string_too_long.plist'))
            self.fail("ASCII string extends into trailer")
        except NotBinaryPlistException as e:
            pass
        except InvalidPlistException as e:
            pass
    
    def testNegativelyLongAsciiString(self):
        try:
            readPlist(fuzz_data_path('ascii_string_negative_length.plist'))
            self.fail("ASCII string length less than zero")
        except NotBinaryPlistException as e:
            pass
        except InvalidPlistException as e:
            pass
    
    def testInvalidOffsetEnding(self):
        # The end of the offset is past the end of the offset table.
        try:
            readPlist(fuzz_data_path('invalid_offset_ending.plist'))
            self.fail("End of offset after end of offset table")
        except NotBinaryPlistException as e:
            pass
        except InvalidPlistException as e:
            pass
    
    def testInvalidDictionaryObjectCount(self):
        try:
            readPlist(fuzz_data_path('dictionary_invalid_count.plist'))
            self.fail("Dictionary object count is not of integer type")
        except NotBinaryPlistException as e:
            pass
        except InvalidPlistException as e:
            pass
    
    def testInvalidArrayObjectCount(self):
        try:
            readPlist(fuzz_data_path('array_invalid_count.plist'))
            self.fail("Array object count is not of integer type")
        except NotBinaryPlistException as e:
            pass
        except InvalidPlistException as e:
            pass
    
    def testInvalidRealLength(self):
        # We shouldn't have been checking for extra length reals, anyway.
        try:
            readPlist(fuzz_data_path('real_invalid_length.plist'))
            self.fail("Real length is not of integer type")
        except NotBinaryPlistException as e:
            pass
        except InvalidPlistException as e:
            pass
    
    def testNaNDateSeconds(self):
        try:
            readPlist(fuzz_data_path('date_seconds_is_nan.plist'))
            self.fail("Date seconds is NaN")
        except NotBinaryPlistException as e:
            pass
        except InvalidPlistException as e:
            pass
    
    def testIntegerWithZeroByteLength(self):
        try:
            readPlist(fuzz_data_path('integer_zero_byte_length.plist'))
            self.fail("Integer has byte size of 0.")
        except NotBinaryPlistException as e:
            pass
        except InvalidPlistException as e:
            pass

