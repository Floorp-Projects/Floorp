# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
from marionette_driver.errors import JavascriptException
from marionette import MarionetteTestCase

class TestLog(MarionetteTestCase):
    def setUp(self):
        MarionetteTestCase.setUp(self)
        self.marionette.enforce_gecko_prefs({"marionette.test.bool": True, "marionette.test.string": "testing", "marionette.test.int": 3})
        self.marionette.set_context('chrome')

    def test_preferences_are_set(self):
        bool_value = self.marionette.execute_script("return Services.prefs.getBoolPref('marionette.test.bool');")
        string_value = self.marionette.execute_script("return Services.prefs.getCharPref('marionette.test.string');")
        int_value = self.marionette.execute_script("return Services.prefs.getIntPref('marionette.test.int');")
        self.assertTrue(bool_value)
        self.assertEqual(string_value, "testing")
        self.assertEqual(int_value, 3)

    def test_change_preset(self):
        bool_value = self.marionette.execute_script("return Services.prefs.getBoolPref('marionette.test.bool');")
        self.assertTrue(bool_value)
        self.marionette.enforce_gecko_prefs({"marionette.test.bool": False})
        self.marionette.set_context('chrome')
        bool_value = self.marionette.execute_script("return Services.prefs.getBoolPref('marionette.test.bool');")
        self.assertFalse(bool_value)

    def test_clean_profile(self):
        self.marionette.restart(clean=True)
        self.marionette.set_context('chrome')
        with self.assertRaisesRegexp(JavascriptException, "NS_ERROR_UNEXPECTED"):
            bool_value = self.marionette.execute_script("return Services.prefs.getBoolPref('marionette.test.bool');")

    def test_can_restart_the_browser(self):
        self.marionette.enforce_gecko_prefs({"marionette.test.restart": True})
        self.marionette.restart()
        self.marionette.set_context('chrome')
        bool_value = self.marionette.execute_script("return Services.prefs.getBoolPref('marionette.test.restart');")
        self.assertTrue(bool_value)

    def test_in_app_restart_the_browser(self):
        self.marionette.execute_script("Services.prefs.setBoolPref('marionette.test.restart', true);")

        # A "soft" restart initiated inside the application should keep track of this pref.
        self.marionette.restart(in_app=True)
        self.marionette.set_context('chrome')
        bool_value = self.marionette.execute_script("""
          return Services.prefs.getBoolPref('marionette.test.restart');
        """)
        self.assertTrue(bool_value)

        bool_value = self.marionette.execute_script("""
          Services.prefs.setBoolPref('marionette.test.restart', false);
          return Services.prefs.getBoolPref('marionette.test.restart');
        """)
        self.assertFalse(bool_value)

        # A "hard" restart is still possible (i.e., our instance is still able
        # to kill the browser).
        # Note we need to clean the profile at this point so the old browser
        # process doesn't interfere with the new one on Windows when it attempts
        # to access the profile on startup.
        self.marionette.restart(clean=True)
