# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import posixpath

from dmunit import DeviceManagerTestCase


class UnzipTestCase(DeviceManagerTestCase):

    def runTest(self):
        """ This tests unzipping a file on the device.
        """
        testroot = posixpath.join(self.dm.getDeviceRoot(), 'infratest')
        self.dm.removeDir(testroot)
        self.dm.mkDir(testroot)
        self.assert_(self.dm.pushFile(
            os.path.join('test-files', 'mybinary.zip'),
            posixpath.join(testroot, 'mybinary.zip')))
        self.assertNotEqual(self.dm.unpackFile(
            posixpath.join(testroot, 'mybinary.zip')), None)
        # the mybinary.zip file has the zipped up contents of test-files/push2
        # so we validate it the same as test_push2.
        self.assert_(self.dm.dirExists(
            posixpath.join(testroot, 'push2', 'sub1')))
        self.assert_(self.dm.validateFile(
            posixpath.join(testroot, 'push2', 'sub1', 'file1.txt'),
            os.path.join('test-files', 'push2', 'sub1', 'file1.txt')))
        self.assert_(self.dm.validateFile(
            posixpath.join(testroot, 'push2', 'sub1', 'sub1.1', 'file2.txt'),
            os.path.join('test-files', 'push2', 'sub1', 'sub1.1', 'file2.txt')))
        self.assert_(self.dm.validateFile(
            posixpath.join(testroot, 'push2', 'sub2', 'file3.txt'),
            os.path.join('test-files', 'push2', 'sub2', 'file3.txt')))
        self.assert_(self.dm.validateFile(
            posixpath.join(testroot, 'push2', 'file4.bin'),
            os.path.join('test-files', 'push2', 'file4.bin')))

        # test dest_dir param
        newdir = posixpath.join(testroot, 'newDir')
        self.dm.mkDir(newdir)

        self.assertNotEqual(self.dm.unpackFile(
            posixpath.join(testroot, 'mybinary.zip'), newdir), None)

        self.assert_(self.dm.dirExists(posixpath.join(newdir, 'push2', 'sub1')))
        self.assert_(self.dm.validateFile(
            posixpath.join(newdir, 'push2', 'sub1', 'file1.txt'),
            os.path.join('test-files', 'push2', 'sub1', 'file1.txt')))
        self.assert_(self.dm.validateFile(
            posixpath.join(newdir, 'push2', 'sub1', 'sub1.1', 'file2.txt'),
            os.path.join('test-files', 'push2', 'sub1', 'sub1.1', 'file2.txt')))
        self.assert_(self.dm.validateFile(
            posixpath.join(newdir, 'push2', 'sub2', 'file3.txt'),
            os.path.join('test-files', 'push2', 'sub2', 'file3.txt')))
        self.assert_(self.dm.validateFile(
            posixpath.join(newdir, 'push2', 'file4.bin'),
            os.path.join('test-files', 'push2', 'file4.bin')))
