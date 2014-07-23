# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
from errors import JavascriptException
from marionette_test import MarionetteTestCase

class TestLog(MarionetteTestCase):
    def setUp(self):
        MarionetteTestCase.setUp(self)
        self.marionette.enforce_gecko_prefs({"marionette.test.bool": True, "marionette.test.string": "testing", "marionette.test.int": 3})

    def test_preferences_are_set(self):
        bool_value = self.marionette.execute_script("return SpecialPowers.getBoolPref('marionette.test.bool');")
        string_value = self.marionette.execute_script("return SpecialPowers.getCharPref('marionette.test.string');")
        int_value = self.marionette.execute_script("return SpecialPowers.getIntPref('marionette.test.int');")
        self.assertTrue(bool_value)
        self.assertEqual(string_value, "testing")
        self.assertEqual(int_value, 3)

    def test_change_preset(self):
        bool_value = self.marionette.execute_script("return SpecialPowers.getBoolPref('marionette.test.bool');")
        self.assertTrue(bool_value)
        self.marionette.enforce_gecko_prefs({"marionette.test.bool": False})
        bool_value = self.marionette.execute_script("return SpecialPowers.getBoolPref('marionette.test.bool');")
        self.assertFalse(bool_value)

    def test_clean_profile(self):
        self.marionette.restart_with_clean_profile()
        with self.assertRaisesRegexp(JavascriptException, "Error getting pref"):
            bool_value = self.marionette.execute_script("return SpecialPowers.getBoolPref('marionette.test.bool');")
