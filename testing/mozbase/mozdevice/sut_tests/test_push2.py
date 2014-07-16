# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import posixpath

from dmunit import DeviceManagerTestCase

class Push2TestCase(DeviceManagerTestCase):

    def runTest(self):
        """This tests copying a directory structure with files to the device.
        """
        testroot = posixpath.join(self.dm.deviceRoot, 'infratest')
        self.dm.removeDir(testroot)
        self.dm.mkDir(testroot)
        path = posixpath.join(testroot, 'push2')
        self.dm.pushDir(os.path.join('test-files', 'push2'), path)

        # Let's walk the tree and make sure everything is there
        # though it's kind of cheesy, we'll use the validate file to compare
        # hashes - we use the client side hashing when testing the cat command
        # specifically, so that makes this a little less cheesy, I guess.
        self.assertTrue(
            self.dm.dirExists(posixpath.join(testroot, 'push2', 'sub1')))
        self.assertTrue(self.dm.validateFile(
            posixpath.join(testroot, 'push2', 'sub1', 'file1.txt'),
            os.path.join('test-files', 'push2', 'sub1', 'file1.txt')))
        self.assertTrue(self.dm.validateFile(
            posixpath.join(testroot, 'push2', 'sub1', 'sub1.1', 'file2.txt'),
            os.path.join('test-files', 'push2', 'sub1', 'sub1.1', 'file2.txt')))
        self.assertTrue(self.dm.validateFile(
            posixpath.join(testroot, 'push2', 'sub2', 'file3.txt'),
            os.path.join('test-files', 'push2', 'sub2', 'file3.txt')))
        self.assertTrue(self.dm.validateFile(
            posixpath.join(testroot, 'push2', 'file4.bin'),
            os.path.join('test-files', 'push2', 'file4.bin')))
