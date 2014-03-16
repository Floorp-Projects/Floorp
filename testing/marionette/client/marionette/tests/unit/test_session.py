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


