# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import posixpath

from dmunit import DeviceManagerTestCase

class PushBinaryTestCase(DeviceManagerTestCase):

    def runTest(self):
        """This tests copying a binary file.
        """
        testroot = self.dm.getDeviceRoot()
        self.dm.removeFile(posixpath.join(testroot, 'mybinary.zip'))
        self.dm.pushFile(os.path.join('test-files', 'mybinary.zip'),
                         posixpath.join(testroot, 'mybinary.zip'))
