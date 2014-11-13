"""
 Info:
   This tests DeviceManagerADB with a real device

 Requirements:
   - You must have a device connected
      - It should be listed under 'adb devices'

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
import re
import socket
import sys
import tempfile
import unittest
from StringIO import StringIO

from mozdevice import DeviceManagerADB, DMError

def find_mount_permissions(dm, mount_path):
    for mount_point in dm._runCmd(["shell", "mount"]).output:
        if mount_point.find(mount_path) > 0:
            return re.search('(ro|rw)(?=,)', mount_point).group(0)

class DeviceManagerADBTestCase(unittest.TestCase):
    tempLocalDir = "tempDir"
    tempLocalFile = os.path.join(tempLocalDir, "tempfile.txt")
    tempRemoteDir = None
    tempRemoteFile = None
    tempRemoteSystemFile = None

    def setUp(self):
        self.assertTrue(find_mount_permissions(self.dm, "/system"), "ro")

        self.assertTrue(os.path.exists(self.tempLocalDir))
        self.assertTrue(os.path.exists(self.tempLocalFile))

        if self.dm.fileExists(self.tempRemoteFile):
            self.dm.removeFile(self.tempRemoteFile)
        self.assertFalse(self.dm.fileExists(self.tempRemoteFile))

        if self.dm.fileExists(self.tempRemoteSystemFile):
            self.dm.removeFile(self.tempRemoteSystemFile)

        self.assertTrue(self.dm.dirExists(self.tempRemoteDir))

    @classmethod
    def setUpClass(self):
        self.dm = DeviceManagerADB()
        if not os.path.exists(self.tempLocalDir):
            os.mkdir(self.tempLocalDir)
        if not os.path.exists(self.tempLocalFile):
            # Create empty file
            open(self.tempLocalFile, 'w').close()
        self.tempRemoteDir = self.dm.getTempDir()
        self.tempRemoteFile = os.path.join(self.tempRemoteDir,
                os.path.basename(self.tempLocalFile))
        self.tempRemoteSystemFile = \
            os.path.join("/system", os.path.basename(self.tempLocalFile))

    @classmethod
    def tearDownClass(self):
        os.remove(self.tempLocalFile)
        os.rmdir(self.tempLocalDir)
        if self.dm.dirExists(self.tempRemoteDir):
            # self.tempRemoteFile will get deleted with it
            self.dm.removeDir(self.tempRemoteDir)
        if self.dm.fileExists(self.tempRemoteSystemFile):
            self.dm.removeFile(self.tempRemoteSystemFile)


class TestFileOperations(DeviceManagerADBTestCase):
    def test_make_and_remove_directory(self):
        dir1 = os.path.join(self.tempRemoteDir, "dir1")
        self.assertFalse(self.dm.dirExists(dir1))
        self.dm.mkDir(dir1)
        self.assertTrue(self.dm.dirExists(dir1))
        self.dm.removeDir(dir1)
        self.assertFalse(self.dm.dirExists(dir1))

    def test_push_and_remove_file(self):
        self.dm.pushFile(self.tempLocalFile, self.tempRemoteFile)
        self.assertTrue(self.dm.fileExists(self.tempRemoteFile))
        self.dm.removeFile(self.tempRemoteFile)
        self.assertFalse(self.dm.fileExists(self.tempRemoteFile))

    def test_push_and_pull_file(self):
        self.dm.pushFile(self.tempLocalFile, self.tempRemoteFile)
        self.assertTrue(self.dm.fileExists(self.tempRemoteFile))
        self.assertFalse(os.path.exists("pulled.txt"))
        self.dm.getFile(self.tempRemoteFile, "pulled.txt")
        self.assertTrue(os.path.exists("pulled.txt"))
        os.remove("pulled.txt")

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

    def test_push_and_remove_system_file(self):
        out = StringIO()
        self.assertTrue(find_mount_permissions(self.dm, "/system") == "ro")
        self.assertFalse(self.dm.fileExists(self.tempRemoteSystemFile))
        self.assertRaises(DMError, self.dm.pushFile, self.tempLocalFile, self.tempRemoteSystemFile)
        self.dm.shell(['mount', '-w', '-o', 'remount', '/system'], out)
        self.assertTrue(find_mount_permissions(self.dm, "/system") == "rw")
        self.assertFalse(self.dm.fileExists(self.tempRemoteSystemFile))
        self.dm.pushFile(self.tempLocalFile, self.tempRemoteSystemFile)
        self.assertTrue(self.dm.fileExists(self.tempRemoteSystemFile))
        self.dm.removeFile(self.tempRemoteSystemFile)
        self.assertFalse(self.dm.fileExists(self.tempRemoteSystemFile))
        self.dm.shell(['mount', '-r', '-o', 'remount', '/system'], out)
        out.close()
        self.assertTrue(find_mount_permissions(self.dm, "/system") == "ro")


class TestOther(DeviceManagerADBTestCase):
    def test_get_list_of_processes(self):
        self.assertEquals(type(self.dm.getProcessList()), list)

    def test_get_current_time(self):
        self.assertEquals(type(self.dm.getCurrentTime()), int)

    def test_get_info(self):
        self.assertEquals(type(self.dm.getInfo()), dict)

    def test_list_devices(self):
        self.assertEquals(len(list(self.dm.devices())), 1)

    def test_shell(self):
        out = StringIO()
        self.dm.shell(["echo", "$COMPANY", ";", "pwd"], out,
                env={"COMPANY":"Mozilla"}, cwd="/", timeout=4, root=True)
        output = str(out.getvalue()).rstrip().splitlines()
        out.close()
        self.assertEquals(output, ['Mozilla', '/'])

    def test_port_forwarding(self):
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.bind(("", 0))
        port = s.getsockname()[1]
        s.close()
        # If successful then no exception is raised
        self.dm.forward("tcp:%s" % port, "tcp:2828")

    def test_port_forwarding_error(self):
        self.assertRaises(DMError, self.dm.forward, "", "")


if __name__ == '__main__':
    dm = DeviceManagerADB()
    if not dm.devices():
       print "There are no connected adb devices"
       sys.exit(1)

    if find_mount_permissions(dm, "/system") == "rw":
        print "We've found out that /system is mounted as 'rw'. This is because the command " \
        "'adb remount' has been run before running this test case. Please reboot the device " \
        "and try again."
        sys.exit(1)

    unittest.main()
