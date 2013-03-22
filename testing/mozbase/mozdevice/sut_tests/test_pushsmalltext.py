# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import posixpath

from dmunit import DeviceManagerTestCase

class PushSmallTextTestCase(DeviceManagerTestCase):

    def runTest(self):
        """This tests copying a small text file.
        """
        testroot = self.dm.getDeviceRoot()
        self.dm.removeFile(posixpath.join(testroot, 'smalltext.txt'))
        self.dm.pushFile(os.path.join('test-files', 'smalltext.txt'),
                         posixpath.join(testroot, 'smalltext.txt'))
