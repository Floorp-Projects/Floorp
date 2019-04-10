# -*- coding: utf-8 -*-

import datetime
import io
import os
import subprocess
import sys
import tempfile
import unittest

from biplist import *
from biplist import PlistWriter
from test_utils import *

try:
    unicode
    unicodeStr = lambda x: x.decode('utf-8')
    toUnicode = lambda x: x.decode('unicode-escape')
except NameError:
    unicode = str
    unicodeStr = lambda x: x
    toUnicode = lambda x: x
try:
    xrange
except NameError:
    xrange = range

class TestWritePlist(unittest.TestCase):
    def setUp(self):
        pass
    
    def roundTrip(self, case, xml=False, expected=None, reprTest=True):
        # reprTest may fail randomly if True and the values being encoded include a dictionary with more
        # than one key.
        
        # convert to plist string
        plist = writePlistToString(case, binary=(not xml))
        self.assertTrue(len(plist) > 0)
        
        # confirm that lint is happy with the result
        self.lintPlist(plist)        
        
        # convert back
        readResult = readPlistFromString(plist)
        
        # test equality
        if reprTest is True:
            self.assertEqual(repr(case if expected is None else expected), repr(readResult))
        else:
            self.assertEqual((case if expected is None else expected), readResult)
        
        # write to file
        plistFile = tempfile.NamedTemporaryFile(mode='wb+', suffix='.plist')
        writePlist(case, plistFile, binary=(xml is False))
        plistFile.seek(0)
        
        # confirm that lint is happy with the result
        self.lintPlist(plistFile)
        
        # read back from file
        fileResult = readPlist(plistFile)
        
        # test equality
        if reprTest is True:
            self.assertEqual(repr(case if expected is None else expected), repr(fileResult))
        else:
            self.assertEqual((case if expected is None else expected), fileResult)
    
    def lintPlist(self, plist):
        if os.access('/usr/bin/plutil', os.X_OK):
            plistFile = None
            plistFilePath = None
            
            if hasattr(plist, 'name'):
                plistFilePath = plist.name
            else:
                if hasattr(plist, 'read'):
                    plistFile = tempfile.NamedTemporaryFile('w%s' % ('b' if 'b' in plist.mode else ''))
                    plistFile.write(plist.read())
                else:
                    plistFile = tempfile.NamedTemporaryFile('w%s' % ('b' if isinstance(plist, bytes) else ''))
                    plistFile.write(plist)
                plistFilePath = plistFile.name
                plistFile.flush()

            status, output = run_command(['/usr/bin/plutil', '-lint', plistFilePath])
            if status != 0:
                self.fail("plutil verification failed (status %d): %s" % (status, output))
    
    def testXMLPlist(self):
        self.roundTrip({'hello':'world'}, xml=True)

    def testXMLPlistWithData(self):
        for binmode in (True, False):
            binplist = writePlistToString({'data': Data(b'\x01\xac\xf0\xff')}, binary=binmode)
            plist = readPlistFromString(binplist)
            self.assertTrue(isinstance(plist['data'], (Data, bytes)), \
                "unable to encode then decode Data into %s plist" % ("binary" if binmode else "XML"))

    def testConvertToXMLPlistWithData(self):
        binplist = writePlistToString({'data': Data(b'\x01\xac\xf0\xff')})
        plist = readPlistFromString(binplist)
        xmlplist = writePlistToString(plist, binary=False)
        self.assertTrue(len(xmlplist) > 0, "unable to convert plist with Data from binary to XML")
    
    def testBoolRoot(self):
        self.roundTrip(True)
        self.roundTrip(False)
    
    def testDuplicate(self):
        l = ["foo" for i in xrange(0, 100)]
        self.roundTrip(l)
        
    def testListRoot(self):
        self.roundTrip([1, 2, 3])
    
    def testDictRoot(self):
        self.roundTrip({'a':1, 'B':'d'}, reprTest=False)
    
    def mixedNumericTypesHelper(self, cases):
        result = readPlistFromString(writePlistToString(cases))
        for i in xrange(0, len(cases)):
            self.assertTrue(cases[i] == result[i])
            self.assertEqual(type(cases[i]), type(result[i]), "Type mismatch on %d: %s != %s" % (i, repr(cases[i]), repr(result[i])))
    
    def testBoolsAndIntegersMixed(self):
        self.mixedNumericTypesHelper([0, 1, True, False, None])
        self.mixedNumericTypesHelper([False, True, 0, 1, None])
        self.roundTrip({'1':[True, False, 1, 0], '0':[1, 2, 0, {'2':[1, 0, False]}]}, reprTest=False)
        self.roundTrip([1, 1, 1, 1, 1, True, True, True, True])
    
    def testFloatsAndIntegersMixed(self):
        self.mixedNumericTypesHelper([0, 1, 1.0, 0.0, None])
        self.mixedNumericTypesHelper([0.0, 1.0, 0, 1, None])
        self.roundTrip({'1':[1.0, 0.0, 1, 0], '0':[1, 2, 0, {'2':[1, 0, 0.0]}]}, reprTest=False)
        self.roundTrip([1, 1, 1, 1, 1, 1.0, 1.0, 1.0, 1.0])
    
    def testSetRoot(self):
        self.roundTrip(set((1, 2, 3)))
    
    def testDatetime(self):
        now = datetime.datetime.utcnow()
        now = now.replace(microsecond=0)
        self.roundTrip([now])
    
    def testFloat(self):
        self.roundTrip({'aFloat':1.23})
    
    def testTuple(self):
        result = writePlistToString({'aTuple':(1, 2.0, 'a'), 'dupTuple':('a', 'a', 'a', 'b', 'b')})
        self.assertTrue(len(result) > 0)
        readResult = readPlistFromString(result)
        self.assertEqual(readResult['aTuple'], [1, 2.0, 'a'])
        self.assertEqual(readResult['dupTuple'], ['a', 'a', 'a', 'b', 'b'])
    
    def testComplicated(self):
        root = {'preference':[1, 2, {'hi there':['a', 1, 2, {'yarrrr':123}]}]}
        self.lintPlist(writePlistToString(root))
        self.roundTrip(root)
    
    def testBytes(self):
        self.roundTrip(b'0')
        self.roundTrip(b'')
        
        self.roundTrip([b'0'])
        self.roundTrip([b''])
        
        self.roundTrip({'a': b'0'})
        self.roundTrip({'a': b''})
    
    def testString(self):
        self.roundTrip('')
        self.roundTrip('a')
        self.roundTrip('1')
        
        self.roundTrip([''])
        self.roundTrip(['a'])
        self.roundTrip(['1'])
        
        self.roundTrip({'a':''})
        self.roundTrip({'a':'a'})
        self.roundTrip({'1':'a'})
        
        self.roundTrip({'a':'a'})
        self.roundTrip({'a':'1'})
    
    def testUnicode(self):
        # defaulting to 1 byte strings
        if str != unicode:
            self.roundTrip(unicodeStr(r''), expected='')
            self.roundTrip(unicodeStr(r'a'), expected='a')
            
            self.roundTrip([unicodeStr(r'a')], expected=['a'])
            
            self.roundTrip({'a':unicodeStr(r'a')}, expected={'a':'a'})
            self.roundTrip({unicodeStr(r'a'):'a'}, expected={'a':'a'})
            self.roundTrip({unicodeStr(r''):unicodeStr(r'')}, expected={'':''})
        
        # TODO: need a 4-byte unicode character
        self.roundTrip(unicodeStr(r'Ã¼'))
        self.roundTrip([unicodeStr(r'Ã¼')])
        self.roundTrip({'a':unicodeStr(r'Ã¼')})
        self.roundTrip({unicodeStr(r'Ã¼'):'a'})
        
        self.roundTrip(toUnicode('\u00b6'))
        self.roundTrip([toUnicode('\u00b6')])
        self.roundTrip({toUnicode('\u00b6'):toUnicode('\u00b6')})
        
        self.roundTrip(toUnicode('\u1D161'))
        self.roundTrip([toUnicode('\u1D161')])
        self.roundTrip({toUnicode('\u1D161'):toUnicode('\u1D161')})
        
        # Smiley face emoji
        self.roundTrip(toUnicode('\U0001f604'))
        self.roundTrip([toUnicode('\U0001f604'), toUnicode('\U0001f604')])
        self.roundTrip({toUnicode('\U0001f604'):toUnicode('\U0001f604')})
    
    def testNone(self):
        self.roundTrip(None)
        self.roundTrip({'1':None})
        self.roundTrip([None, None, None])
    
    def testBools(self):
        self.roundTrip(True)
        self.roundTrip(False)
        
        self.roundTrip([True, False])
        
        self.roundTrip({'a':True, 'b':False}, reprTest=False)
    
    def testUniques(self):
        root = {'hi':'there', 'halloo':'there'}
        self.roundTrip(root, reprTest=False)
    
    def testAllEmpties(self):
        '''Primarily testint that an empty unicode and bytes are not mixed up'''
        self.roundTrip([unicodeStr(''), '', b'', [], {}], expected=['', '', b'', [], {}])
    
    def testLargeDict(self):
        d = dict((str(x), str(x)) for x in xrange(0, 1000))
        self.roundTrip(d, reprTest=False)
        
    def testWriteToFile(self):
        for is_binary in [True, False]:
            with tempfile.NamedTemporaryFile(mode='w%s' % ('b' if is_binary else ''), suffix='.plist') as plistFile:
                # clear out the created file
                os.unlink(plistFile.name)
                self.assertFalse(os.path.exists(plistFile.name))
                
                # write to disk
                writePlist([1, 2, 3], plistFile.name, binary=is_binary)
                self.assertTrue(os.path.exists(plistFile.name))
                
                with open(plistFile.name, 'r%s' % ('b' if is_binary else '')) as f:
                    fileContents = f.read()
                    self.lintPlist(fileContents)
    
    def testBadKeys(self):
        try:
            self.roundTrip({None:1})
            self.fail("None is not a valid key in Cocoa.")
        except InvalidPlistException as e:
            pass
        try:
            self.roundTrip({Data(b"hello world"):1})
            self.fail("Data is not a valid key in Cocoa.")
        except InvalidPlistException as e:
            pass
        try:
            self.roundTrip({1:1})
            self.fail("Number is not a valid key in Cocoa.")
        except InvalidPlistException as e:
            pass
    
    def testIntBoundaries(self):
        edges = [0xff, 0xffff, 0xffffffff]
        for edge in edges:
            cases = [edge, edge-1, edge+1, edge-2, edge+2, edge*2, edge/2]
            self.roundTrip(cases)
        edges = [-pow(2, 7), pow(2, 7) - 1, 
                 -pow(2, 15), pow(2, 15) - 1, 
                 -pow(2, 31), pow(2, 31) - 1, 
                 -pow(2, 63), pow(2, 64) - 1]
        self.roundTrip(edges, reprTest=False)
        
        ioBytes = io.BytesIO()
        writer = PlistWriter(ioBytes)
        bytes = [(1, [pow(2, 7) - 1]),
                 (2, [pow(2, 15) - 1]),
                 (4, [pow(2, 31) - 1]),
                 (8, [-pow(2, 7), -pow(2, 15), -pow(2, 31), -pow(2, 63), pow(2, 63) - 1]),
                 (16, [pow(2, 64) - 1])
            ]
        for bytelen, tests in bytes:
            for test in tests:
                got = writer.intSize(test)
                self.assertEqual(bytelen, got, "Byte size is wrong. Expected %d, got %d" % (bytelen, got))
        
        bytes_lists = [list(x) for x in bytes]
        self.roundTrip(bytes_lists, reprTest=False)
        
        try:
            self.roundTrip([0x10000000000000000, pow(2, 64)])
            self.fail("2^64 should be too large for Core Foundation to handle.")
        except InvalidPlistException as e:
            pass
    
    def testUnicode2(self):
        unicodeRoot = toUnicode("Mirror's Edge\u2122 for iPad")
        self.roundTrip(unicodeRoot)
        unicodeStrings = [toUnicode("Mirror's Edge\u2122 for iPad"), toUnicode('Weightbot \u2014 Track your Weight in Style')]
        self.roundTrip(unicodeStrings)
        self.roundTrip({toUnicode(""):toUnicode("")}, expected={'':''})
        self.roundTrip(toUnicode(""), expected='')
    
    def testWriteData(self):
        self.roundTrip(Data(b"woohoo"))

    def testEmptyData(self):
        data = Data(b'')
        binplist = writePlistToString(data)
        plist = readPlistFromString(binplist)
        self.assertEqual(plist, data)
        self.assertEqual(type(plist), type(data))
        
    def testUidWrite(self):
        self.roundTrip({'$version': 100000, 
            '$objects': 
                ['$null', 
                 {'$class': Uid(3), 'somekey': Uid(2)}, 
                 'object value as string', 
                 {'$classes': ['Archived', 'NSObject'], '$classname': 'Archived'}
                 ], 
            '$top': {'root': Uid(1)}, '$archiver': 'NSKeyedArchiver'}, reprTest=False)
    
    def testUidRoundTrip(self):
        # Per https://github.com/wooster/biplist/issues/9
        self.roundTrip(Uid(1))
        self.roundTrip([Uid(1), 1])
        self.roundTrip([1, Uid(1)])
        self.roundTrip([Uid(1), Uid(1)])
    
    def testRecursiveWrite(self):
        # Apple libraries disallow recursive containers, so we should fail on
        # trying to write those.
        root = []
        child = [root]
        root.extend(child)
        try:
            writePlistToString(root)
            self.fail("Should not be able to write plists with recursive containers.")
        except InvalidPlistException as e:
            pass
        except:
            self.fail("Should get an invalid plist exception for recursive containers.")

if __name__ == '__main__':
    unittest.main()
