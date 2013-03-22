# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from mozdevice import devicemanager
from mozdevice import devicemanagerSUT
import types
import unittest

ip = ''
port = 0
heartbeat_port = 0


class DeviceManagerTestCase(unittest.TestCase):
    """DeviceManager tests should subclass this.
    """

    """Set to False in your derived class if this test
    should not be run on the Python agent.
    """
    runs_on_test_device = True

    def _setUp(self):
        """ Override this if you want set-up code in your test."""
        return

    def setUp(self):
        self.dm = devicemanagerSUT.DeviceManagerSUT(host=ip, port=port)
        self.dmerror = devicemanager.DMError
        self.nettools = devicemanager.NetworkTools
        self._setUp()


class DeviceManagerTestLoader(unittest.TestLoader):

    def __init__(self, isTestDevice=False):
        self.isTestDevice = isTestDevice

    def loadTestsFromModuleName(self, module_name):
        """Loads tests from modules unless the SUT is a test device and
        the test case has runs_on_test_device set to False
        """
        tests = []
        module = __import__(module_name)
        for name in dir(module):
            obj = getattr(module, name)
            if (isinstance(obj, (type, types.ClassType)) and
                issubclass(obj, unittest.TestCase)) and \
                (not self.isTestDevice or obj.runs_on_test_device):
                tests.append(self.loadTestsFromTestCase(obj))
        return self.suiteClass(tests)
