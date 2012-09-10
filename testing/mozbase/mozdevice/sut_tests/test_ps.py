# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import re

from dmunit import DeviceManagerTestCase


class ProcessListTestCase(DeviceManagerTestCase):

    def runTest(self):
        """ This tests getting a process list from the device
        """
        proclist = self.dm.getProcessList()

        # This returns a process list of the form:
        # [[<procid>,<procname>],[<procid>,<procname>]...]
        # on android the userID is affixed to the process array:
        # [[<procid>, <procname>, <userid>]...]
        procid = re.compile('^[a-f0-9]+')
        procname = re.compile('.+')

        self.assertNotEqual(len(proclist), 0)

        for item in proclist:
            self.assert_(procid.match(item[0]))
            self.assert_(procname.match(item[1]))
