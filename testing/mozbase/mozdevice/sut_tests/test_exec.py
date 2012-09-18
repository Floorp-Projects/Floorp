# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from StringIO import StringIO
import posixpath

from dmunit import DeviceManagerTestCase


class ProcessListTestCase(DeviceManagerTestCase):

    def runTest(self):
        """ simple exec test, does not use env vars """
        out = StringIO()
        filename = posixpath.join(self.dm.getDeviceRoot(), 'test_exec_file')
        # make sure the file was not already there
        self.dm.removeFile(filename)
        self.dm.shell(['touch', filename], out)
        # check that the file has been created
        self.assertTrue(self.dm.fileExists(filename))
        # clean up
        self.dm.removeFile(filename)
