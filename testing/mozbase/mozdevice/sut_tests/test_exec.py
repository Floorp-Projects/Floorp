# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import posixpath
from StringIO import StringIO

from dmunit import DeviceManagerTestCase

class ExecTestCase(DeviceManagerTestCase):

    def runTest(self):
        """Simple exec test, does not use env vars."""
        out = StringIO()
        filename = posixpath.join(self.dm.deviceRoot, 'test_exec_file')
        # Make sure the file was not already there
        self.dm.removeFile(filename)
        self.dm.shell(['dd', 'if=/dev/zero', 'of=%s' % filename, 'bs=1024',
                       'count=1'], out)
        # Check that the file has been created
        self.assertTrue(self.dm.fileExists(filename))
        # Clean up
        self.dm.removeFile(filename)
