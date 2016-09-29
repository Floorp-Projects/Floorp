# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette import MarionetteTestCase


class TestProfileManagement(MarionetteTestCase):

    def setUp(self):
        MarionetteTestCase.setUp(self)
        self.marionette.enforce_gecko_prefs(
            {"marionette.test.bool": True,
             "marionette.test.string": "testing",
             "marionette.test.int": 3
             })
        self.marionette.set_context("chrome")

    def test_preferences_are_set(self):
        self.assertTrue(self.marionette.get_pref("marionette.test.bool"))
        self.assertEqual(self.marionette.get_pref("marionette.test.string"), "testing")
        self.assertEqual(self.marionette.get_pref("marionette.test.int"), 3)

    def test_change_preference(self):
        self.assertTrue(self.marionette.get_pref("marionette.test.bool"))

        self.marionette.enforce_gecko_prefs({"marionette.test.bool": False})

        self.assertFalse(self.marionette.get_pref("marionette.test.bool"))

    def test_clean_profile(self):
        self.marionette.restart(clean=True)

        self.assertEqual(self.marionette.get_pref("marionette.test.bool"), None)
