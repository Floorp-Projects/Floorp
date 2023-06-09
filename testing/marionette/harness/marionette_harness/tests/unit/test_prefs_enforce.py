# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import six

from marionette_harness import MarionetteTestCase


class TestEnforcePreferences(MarionetteTestCase):
    def setUp(self):
        super(TestEnforcePreferences, self).setUp()
        self.marionette.set_context("chrome")

    def tearDown(self):
        self.marionette.restart(in_app=False, clean=True)

        super(TestEnforcePreferences, self).tearDown()

    def enforce_prefs(self, prefs=None):
        test_prefs = {
            "marionette.test.bool": True,
            "marionette.test.int": 3,
            "marionette.test.string": "testing",
        }

        self.marionette.enforce_gecko_prefs(prefs or test_prefs)

    def test_preferences_are_set(self):
        self.enforce_prefs()
        self.assertTrue(self.marionette.get_pref("marionette.test.bool"))
        self.assertEqual(self.marionette.get_pref("marionette.test.string"), "testing")
        self.assertEqual(self.marionette.get_pref("marionette.test.int"), 3)

    def test_change_enforced_preference(self):
        self.enforce_prefs()
        self.assertTrue(self.marionette.get_pref("marionette.test.bool"))

        self.enforce_prefs({"marionette.test.bool": False})
        self.assertFalse(self.marionette.get_pref("marionette.test.bool"))

    def test_restart_with_clean_profile_after_enforce_prefs(self):
        self.enforce_prefs()
        self.assertTrue(self.marionette.get_pref("marionette.test.bool"))

        self.marionette.restart(in_app=False, clean=True)
        self.assertEqual(self.marionette.get_pref("marionette.test.bool"), None)

    def test_restart_preserves_requested_capabilities(self):
        self.marionette.delete_session()
        self.marionette.start_session(capabilities={"test:fooBar": True})

        self.enforce_prefs()
        self.assertEqual(self.marionette.session.get("test:fooBar"), True)
