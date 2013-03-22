# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import posixpath

from dmunit import DeviceManagerTestCase

class Push1TestCase(DeviceManagerTestCase):

    def runTest(self):
        """This tests copying a directory structure to the device.
        """
        dvroot = self.dm.getDeviceRoot()
        dvpath = posixpath.join(dvroot, 'infratest')
        self.dm.removeDir(dvpath)
        self.dm.mkDir(dvpath)

        p1 = os.path.join('test-files', 'push1')
        # Set up local stuff
        try:
            os.rmdir(p1)
        except:
            pass

        if not os.path.exists(p1):
            os.makedirs(os.path.join(p1, 'sub.1', 'sub.2'))
        if not os.path.exists(os.path.join(p1, 'sub.1', 'sub.2', 'testfile')):
            file(os.path.join(p1, 'sub.1', 'sub.2', 'testfile'), 'w').close()

        self.dm.pushDir(p1, posixpath.join(dvpath, 'push1'))

        self.assertTrue(
            self.dm.dirExists(posixpath.join(dvpath, 'push1', 'sub.1')))
        self.assertTrue(self.dm.dirExists(
            posixpath.join(dvpath, 'push1', 'sub.1', 'sub.2')))
