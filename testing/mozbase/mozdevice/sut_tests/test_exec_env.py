# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from StringIO import StringIO
import os
import posixpath

from dmunit import DeviceManagerTestCase


class ProcessListTestCase(DeviceManagerTestCase):

    def runTest(self):
        """ simple exec test, does not use env vars """
        # push the file
        localfile = os.path.join('test-files', 'test_script.sh')
        remotefile = posixpath.join(self.dm.getDeviceRoot(), 'test_script.sh')
        self.dm.pushFile(localfile, remotefile)

        # run the cmd
        out = StringIO()
        self.dm.shell(['sh', remotefile], out, env={'THE_ANSWER': 42})

        # rewind the output file
        out.seek(0)
        # make sure first line is 42
        line = out.readline()
        self.assertTrue(int(line) == 42)

        # clean up
        self.dm.removeFile(remotefile)
