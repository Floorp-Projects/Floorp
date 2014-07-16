# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import posixpath
import shutil
import tempfile

from mozdevice.devicemanager import DMError
from dmunit import DeviceManagerTestCase

class GetDirectoryTestCase(DeviceManagerTestCase):

    def _setUp(self):
        self.localsrcdir = tempfile.mkdtemp()
        os.makedirs(os.path.join(self.localsrcdir, 'push1', 'sub.1', 'sub.2'))
        path = os.path.join(self.localsrcdir,
                            'push1', 'sub.1', 'sub.2', 'testfile')
        file(path, 'w').close()
        os.makedirs(os.path.join(self.localsrcdir, 'push1', 'emptysub'))
        self.localdestdir = tempfile.mkdtemp()
        self.expected_filelist = ['emptysub', 'sub.1']

    def tearDown(self):
        shutil.rmtree(self.localsrcdir)
        shutil.rmtree(self.localdestdir)

    def runTest(self):
        """This tests the getDirectory() function.
        """
        testroot = posixpath.join(self.dm.deviceRoot, 'infratest')
        self.dm.removeDir(testroot)
        self.dm.mkDir(testroot)
        self.dm.pushDir(
            os.path.join(self.localsrcdir, 'push1'),
            posixpath.join(testroot, 'push1'))
        # pushDir doesn't copy over empty directories, but we want to make sure
        # that they are retrieved correctly.
        self.dm.mkDir(posixpath.join(testroot, 'push1', 'emptysub'))
        self.dm.getDirectory(posixpath.join(testroot, 'push1'),
                             os.path.join(self.localdestdir, 'push1'))
        self.assertTrue(os.path.exists(
            os.path.join(self.localdestdir,
                         'push1', 'sub.1', 'sub.2', 'testfile')))
        self.assertTrue(os.path.exists(
            os.path.join(self.localdestdir, 'push1', 'emptysub')))
        self.assertRaises(DMError, self.dm.getDirectory,
                '/dummy', os.path.join(self.localdestdir, '/none'))
        self.assertFalse(os.path.exists(self.localdestdir + '/none'))
