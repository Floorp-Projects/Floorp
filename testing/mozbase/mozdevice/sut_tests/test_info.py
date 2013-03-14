# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from dmunit import DeviceManagerTestCase

class InfoTestCase(DeviceManagerTestCase):

    runs_on_test_device = False

    def runTest(self):
        """This tests the "info" command.
        """
        cmds = ('os', 'id', 'systime', 'uptime', 'screen', 'memory', 'power')
        for c in cmds:
            data = self.dm.getInfo(c)
            print c + str(data)

        # No real good way to verify this.  If it doesn't throw, we're ok.
