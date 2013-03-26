# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import re
import socket
from time import strptime

from dmunit import DeviceManagerTestCase, heartbeat_port

class DataChannelTestCase(DeviceManagerTestCase):

    runs_on_test_device = False

    def runTest(self):
        """This tests the heartbeat and the data channel.
        """
        ip = self.dm.host

        # Let's connect
        self._datasock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        # Assume 60 seconds between heartbeats
        self._datasock.settimeout(float(60 * 2))
        self._datasock.connect((ip, heartbeat_port))
        self._connected = True

        # Let's listen
        numbeats = 0
        capturedHeader = False
        while numbeats < 3:
            data = self._datasock.recv(1024)
            print data
            self.assertNotEqual(len(data), 0)

            # Check for the header
            if not capturedHeader:
                m = re.match(r"(.*?) trace output", data)
                self.assertNotEqual(m, None,
                    'trace output line does not match. The line: ' + str(data))
                capturedHeader = True

            # Check for standard heartbeat messsage
            m = re.match(r"(.*?) Thump thump - (.*)", data)
            if m == None:
                # This isn't an error, it usually means we've obtained some
                # unexpected data from the device
                continue

            # Ensure it matches our format
            mHeartbeatTime = m.group(1)
            mHeartbeatTime = strptime(mHeartbeatTime, "%Y%m%d-%H:%M:%S")
            numbeats = numbeats + 1
