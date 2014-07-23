"""
 Info:
   This tests DeviceManagerADB with a real device

 Requirements:
   - You must have a device connected

 Notes:
   - Not all functions have been covered.
     In particular, functions from the parent class
   - No testing of properties is done
   - The test case are very simple and it could be
     done with deeper inspection of the return values

 Author(s):
   - Armen Zambrano <armenzg@mozilla.com>

 Functions that are not being tested:
 - launchProcess - DEPRECATED
 - getIP
 - recordLogcat
 - saveScreenshot
 - validateDir
 - mkDirs
 - getDeviceRoot
 - shellCheckOutput
 - processExist

 I assume these functions are only useful for Android
 - getAppRoot()
 - updateApp()
 - uninstallApp()
 - uninstallAppAndReboot()
"""

import os
import sys
import tempfile
import unittest
from mozdevice import DeviceManagerADB
from StringIO import StringIO

class DeviceManagerADBTestCase(unittest.TestCase):
    tempLocalDir = "tempDir"
    tempLocalFile = os.path.join(tempLocalDir, "tempfile.txt")
    tempRemoteDir = None
    tempRemoteFile = None

    def setUp(self):
        self.dm = DeviceManagerADB()
        if not os.path.exists(self.tempLocalDir):
            os.mkdir(self.tempLocalDir)
        if not os.path.exists(self.tempLocalFile):
            # Create empty file
            open(self.tempLocalFile, 'w').close()
        self.tempRemoteDir = self.dm.getTempDir()
        self.tempRemoteFile = os.path.join(self.tempRemoteDir,
                os.path.basename(self.tempLocalFile))

    def tearDown(self):
        os.remove(self.tempLocalFile)
        os.rmdir(self.tempLocalDir)
        if self.dm.dirExists(self.tempRemoteDir):
            self.dm.removeDir(self.tempRemoteDir)


class TestFileOperations(DeviceManagerADBTestCase):
    def test_make_and_remove_directory(self):
        self.assertTrue(self.dm.dirExists(self.tempRemoteDir))
        dir1 = os.path.join(self.tempRemoteDir, "dir1")
        self.assertFalse(self.dm.dirExists(dir1))
        self.dm.mkDir(dir1)
        self.assertTrue(self.dm.dirExists(dir1))
        self.dm.removeDir(dir1)
        self.assertFalse(self.dm.dirExists(dir1))

    def test_push_and_remove_file(self):
        self.assertFalse(self.dm.fileExists(self.tempRemoteFile))
        self.dm.pushFile(self.tempLocalFile, self.tempRemoteFile)
        self.assertTrue(self.dm.fileExists(self.tempRemoteFile))
        self.dm.removeFile(self.tempRemoteFile)
        self.assertFalse(self.dm.fileExists(self.tempRemoteFile))

    def test_push_and_pull_file(self):
        self.assertFalse(self.dm.fileExists(self.tempRemoteFile))
        self.dm.pushFile(self.tempLocalFile, self.tempRemoteFile)
        self.assertTrue(self.dm.fileExists(self.tempRemoteFile))
        self.assertFalse(os.path.exists("pulled.txt"))
        self.dm.getFile(self.tempRemoteFile, "pulled.txt")
        self.assertTrue(os.path.exists("pulled.txt"))
        os.remove("pulled.txt")

    def test_remove_file(self):
        # Already tested on test_push_and_remove_file()
        pass

    def test_push_and_pull_directory_and_list_files(self):
        self.dm.removeDir(self.tempRemoteDir)
        self.assertFalse(self.dm.dirExists(self.tempRemoteDir))
        self.dm.pushDir(self.tempLocalDir, self.tempRemoteDir)
        self.assertTrue(self.dm.dirExists(self.tempRemoteDir))
        response = self.dm.listFiles(self.tempRemoteDir)
        # The local dir that was pushed contains the tempLocalFile
        self.assertIn(os.path.basename(self.tempLocalFile), response)
        # Create a temp dir to pull to
        temp_dir = tempfile.mkdtemp()
        self.assertTrue(os.path.exists(temp_dir))
        self.dm.getDirectory(self.tempRemoteDir, temp_dir)
        self.assertTrue(os.path.exists(self.tempLocalFile))

    def test_move_and_remove_directories(self):
        dir1 = os.path.join(self.tempRemoteDir, "dir1")
        dir2 = os.path.join(self.tempRemoteDir, "dir2")

        self.assertFalse(self.dm.dirExists(dir1))
        self.dm.mkDir(dir1)
        self.assertTrue(self.dm.dirExists(dir1))

        self.assertFalse(self.dm.dirExists(dir2))
        self.dm.moveTree(dir1, dir2)
        self.assertTrue(self.dm.dirExists(dir2))

        self.dm.removeDir(dir1)
        self.dm.removeDir(dir2)
        self.assertFalse(self.dm.dirExists(dir1))
        self.assertFalse(self.dm.dirExists(dir2))

class TestOther(DeviceManagerADBTestCase):
    def test_get_list_of_processes(self):
        self.assertEquals(type(self.dm.getProcessList()), list)

    def test_get_current_time(self):
        self.assertEquals(type(self.dm.getCurrentTime()), int)

    def test_get_info(self):
        self.assertEquals(self.dm.getInfo(), {})
        # Commented since it is too nosiy
        #self.assertEquals(self.dm.getInfo("all"), dict)

    def test_remount(self):
        self.assertEquals(self.dm.remount(), 0)

    def test_list_devices(self):
        self.assertEquals(len(list(self.dm.devices())), 1)

    def test_shell(self):
        out = StringIO()
        self.dm.shell(["echo", "$COMPANY", ";", "pwd"], out,
                env={"COMPANY":"Mozilla"}, cwd="/", timeout=4, root=True)
        output = str(out.getvalue()).rstrip().splitlines()
        out.close()
        self.assertEquals(output, ['Mozilla', '/'])

    def test_reboot(self):
        # We are not running the reboot function because it takes too long
        #self.dm.reboot(wait=True)
        pass

    def test_port_forwarding(self):
        # I don't really know how to test this properly
        self.assertEquals(self.dm.forward("tcp:2828", "tcp:2828"), 0)

if __name__ == '__main__':
    dm = DeviceManagerADB()
    if not dm.devices():
       print "There are no connected adb devices"
       sys.exit(1)

    unittest.main()
