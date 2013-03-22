# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import posixpath
from StringIO import StringIO

from dmunit import DeviceManagerTestCase

class ExecEnvTestCase(DeviceManagerTestCase):

    def runTest(self):
        """Exec test with env vars."""
        # Push the file
        localfile = os.path.join('test-files', 'test_script.sh')
        remotefile = posixpath.join(self.dm.getDeviceRoot(), 'test_script.sh')
        self.dm.pushFile(localfile, remotefile)

        # Run the cmd
        out = StringIO()
        self.dm.shell(['sh', remotefile], out, env={'THE_ANSWER': 42})

        # Rewind the output file
        out.seek(0)
        # Make sure first line is 42
        line = out.readline()
        self.assertTrue(int(line) == 42)

        # Clean up
        self.dm.removeFile(remotefile)
