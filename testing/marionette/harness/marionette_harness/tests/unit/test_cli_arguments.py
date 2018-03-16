# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

import copy
import platform
try:
    import winreg
except ImportError:
    try:
        import _winreg as winreg
    except ImportError:
        pass

from marionette_harness import MarionetteTestCase


class TestCommandLineArguments(MarionetteTestCase):

    def setUp(self):
        super(TestCommandLineArguments, self).setUp()

        self.orig_arguments = copy.copy(self.marionette.instance.app_args)

    def tearDown(self):
        self.marionette.instance.app_args = self.orig_arguments
        self.marionette.quit(clean=True)

        super(TestCommandLineArguments, self).tearDown()

    def test_start_in_safe_mode(self):
        self.marionette.instance.app_args.append("-safe-mode")

        self.marionette.quit()
        self.marionette.start_session()

        with self.marionette.using_context("chrome"):
            safe_mode = self.marionette.execute_script("""
              Cu.import("resource://gre/modules/Services.jsm");

              return Services.appinfo.inSafeMode;
            """)
            self.assertTrue(safe_mode, "Safe Mode has not been enabled")

    def test_safe_mode_blocked_by_policy(self):
        if platform.system() != 'Windows':
            return

        reg_policies = winreg.OpenKeyEx(winreg.HKEY_CURRENT_USER, "SOFTWARE\\Policies", 0, winreg.KEY_WRITE)
        reg_mozilla = winreg.CreateKeyEx(reg_policies, "Mozilla", 0, winreg.KEY_WRITE)
        reg_firefox = winreg.CreateKeyEx(reg_mozilla, "Firefox", 0, winreg.KEY_WRITE)
        winreg.SetValueEx(reg_firefox, "DisableSafeMode", 0, winreg.REG_DWORD, 1)

        self.marionette.instance.app_args.append("-safe-mode")

        self.marionette.quit()
        self.marionette.start_session()

        with self.marionette.using_context("chrome"):
            safe_mode = self.marionette.execute_script("""
              Cu.import("resource://gre/modules/Services.jsm");

              return Services.appinfo.inSafeMode;
            """)
            self.assertFalse(safe_mode, "Safe Mode has been enabled")

        winreg.CloseKey(reg_firefox)
        winreg.DeleteKey(reg_mozilla, "Firefox")
        winreg.CloseKey(reg_mozilla)
        winreg.DeleteKey(reg_policies, "Mozilla")
        winreg.CloseKey(reg_policies)

    def test_startup_timeout(self):
        startup_timeout = self.marionette.startup_timeout

        # Use a timeout which always cause an IOError
        self.marionette.startup_timeout = .1
        msg = "Process killed after {}s".format(self.marionette.startup_timeout)

        try:
            self.marionette.quit()
            with self.assertRaisesRegexp(IOError, msg):
                self.marionette.start_session()
        finally:
            self.marionette.startup_timeout = startup_timeout
            self.marionette.start_session()
