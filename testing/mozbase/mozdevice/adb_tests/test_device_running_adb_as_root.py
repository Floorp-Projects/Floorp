"""
 This test is to test devices that adbd does not get started as root.
 Specifically devices that have ro.secure == 1 and ro.debuggable == 1

 Running this test case requires various reboots which makes it a
 very slow test case to run.
"""
import unittest
import sys

from mozdevice import DeviceManagerADB


class TestFileOperations(unittest.TestCase):

    def setUp(self):
        dm = DeviceManagerADB()
        dm.reboot(wait=True)

    def test_run_adb_as_root_parameter(self):
        dm = DeviceManagerADB()
        self.assertTrue(dm.processInfo("adbd")[2] != "root")
        dm = DeviceManagerADB(runAdbAsRoot=True)
        self.assertTrue(dm.processInfo("adbd")[2] == "root")

    def test_after_reboot_adb_runs_as_root(self):
        dm = DeviceManagerADB(runAdbAsRoot=True)
        self.assertTrue(dm.processInfo("adbd")[2] == "root")
        dm.reboot(wait=True)
        self.assertTrue(dm.processInfo("adbd")[2] == "root")

    def tearDown(self):
        dm = DeviceManagerADB()
        dm.reboot()


if __name__ == "__main__":
    dm = DeviceManagerADB()
    if not dm.devices():
        print "There are no connected adb devices"
        sys.exit(1)
    else:
        if not (int(dm._runCmd(["shell", "getprop", "ro.secure"]).output[0]) and
                int(dm._runCmd(["shell", "getprop", "ro.debuggable"]).output[0])):
            print "This test case is meant for devices with devices that start " \
                "adbd as non-root and allows for adbd to be restarted as root."
            sys.exit(1)

    unittest.main()
