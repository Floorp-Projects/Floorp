# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import marionette_test

class TestSession(marionette_test.MarionetteTestCase):
    def setUp(self):
        super(TestSession, self).setUp()
        self.marionette.delete_session()

    def test_new_session_returns_capabilities(self):
        # Sends newSession
        caps = self.marionette.start_session()

        # Check that session was created.  This implies the server
        # sent us the sessionId and status fields.
        self.assertIsNotNone(self.marionette.session)

        # Required capabilities mandated by WebDriver spec
        self.assertIn("browserName", caps)
        self.assertIn("platformName", caps)
        self.assertIn("platformVersion", caps)

        # Optional capabilities we want Marionette to support
        self.assertIn("cssSelectorsEnabled", caps)
        self.assertIn("device", caps)
        self.assertIn("handlesAlerts", caps)
        self.assertIn("javascriptEnabled", caps)
        self.assertIn("rotatable", caps)
        self.assertIn("takesScreenshot", caps)
        self.assertIn("version", caps)

class TestCapabilities(marionette_test.MarionetteTestCase):
    def setUp(self):
        super(TestCapabilities, self).setUp()
        self.caps = self.marionette.session_capabilities
        self.marionette.set_context("chrome")
        self.appinfo = self.marionette.execute_script(
            "return Services.appinfo")

    def test_mandates_capabilities(self):
        self.assertIn("browserName", self.caps)
        self.assertIn("browserVersion", self.caps)
        self.assertIn("platformName", self.caps)
        self.assertIn("platformVersion", self.caps)

        self.assertEqual(self.caps["browserName"], self.appinfo["name"])
        self.assertEqual(self.caps["browserVersion"], self.appinfo["version"])
        self.assertEqual(self.caps["platformName"], self.appinfo["OS"].upper())
        self.assertEqual(self.caps["platformVersion"],
                         self.appinfo["platformVersion"])

    def test_supported_features(self):
        self.assertIn("cssSelectorsEnabled", self.caps)
        self.assertIn("handlesAlerts", self.caps)
        self.assertIn("javascriptEnabled", self.caps)
        self.assertIn("nativeEvents", self.caps)
        self.assertIn("rotatable", self.caps)
        self.assertIn("secureSsl", self.caps)
        self.assertIn("takesElementScreenshot", self.caps)
        self.assertIn("takesScreenshot", self.caps)

        self.assertTrue(self.caps["cssSelectorsEnabled"])
        self.assertFalse(self.caps["handlesAlerts"])
        self.assertTrue(self.caps["javascriptEnabled"])
        self.assertFalse(self.caps["nativeEvents"])
        self.assertEqual(self.caps["rotatable"], self.appinfo["name"] == "B2G")
        self.assertFalse(self.caps["secureSsl"])
        self.assertTrue(self.caps["takesElementScreenshot"])
        self.assertTrue(self.caps["takesScreenshot"])

    def test_selenium2_compat(self):
        self.assertIn("platform", self.caps)
        self.assertEqual(self.caps["platform"], self.caps["platformName"])

    def test_extensions(self):
        self.assertIn("XULappId", self.caps)
        self.assertIn("appBuildId", self.caps)
        self.assertIn("device", self.caps)
        self.assertIn("version", self.caps)

        self.assertEqual(self.caps["XULappId"], self.appinfo["ID"])
        self.assertEqual(self.caps["appBuildId"], self.appinfo["appBuildID"])
        self.assertEqual(self.caps["version"], self.appinfo["version"])
