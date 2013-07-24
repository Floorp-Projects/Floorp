# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import tempfile
import posixpath

from dmunit import DeviceManagerTestCase

class FileExistsTestCase(DeviceManagerTestCase):
    """This tests the "fileExists" command.
    """

    def testOnRoot(self):
        self.assertTrue(self.dm.fileExists('/'))

    def testOnNonexistent(self):
        self.assertFalse(self.dm.fileExists('/doesNotExist'))

    def testOnRegularFile(self):
        remote_path = posixpath.join(self.dm.getDeviceRoot(), 'testFile')
        self.assertFalse(self.dm.fileExists(remote_path))
        with tempfile.NamedTemporaryFile() as f:
            self.dm.pushFile(f.name, remote_path)
        self.assertTrue(self.dm.fileExists(remote_path))
        self.dm.removeFile(remote_path)

    def testOnDirectory(self):
        remote_path = posixpath.join(self.dm.getDeviceRoot(), 'testDir')
        remote_path_file = posixpath.join(remote_path, 'testFile')
        self.assertFalse(self.dm.fileExists(remote_path))
        with tempfile.NamedTemporaryFile() as f:
            self.dm.pushFile(f.name, remote_path_file)
        self.assertTrue(self.dm.fileExists(remote_path))
        self.dm.removeFile(remote_path_file)
        self.dm.removeDir(remote_path)

