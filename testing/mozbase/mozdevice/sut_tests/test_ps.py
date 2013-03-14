# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from dmunit import DeviceManagerTestCase

class ProcessListTestCase(DeviceManagerTestCase):

    def runTest(self):
        """This tests getting a process list from the device.
        """
        proclist = self.dm.getProcessList()

        # This returns a process list of the form:
        # [[<procid>, <procname>], [<procid>, <procname>], ...]
        # on android the userID is affixed to the process array:
        # [[<procid>, <procname>, <userid>], ...]

        self.assertNotEqual(len(proclist), 0)

        for item in proclist:
            self.assertIsInstance(item[0], int)
            self.assertIsInstance(item[1], str)
            self.assertGreater(len(item[1]), 0)
            if len(item) > 2:
                self.assertIsInstance(item[2], int)

