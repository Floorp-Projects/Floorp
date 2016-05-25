# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette import MarionetteTestCase
from marionette_driver.errors import SessionNotCreatedException

class TestSession(MarionetteTestCase):
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
        self.assertIn("device", caps)
        self.assertIn("rotatable", caps)
        self.assertIn("takesScreenshot", caps)
        self.assertIn("version", caps)

    def test_we_can_get_the_session_id(self):
        # Sends newSession
        self.marionette.start_session()

        self.assertTrue(self.marionette.session_id is not None)
        self.assertTrue(isinstance(self.marionette.session_id, unicode))

    def test_we_can_set_the_session_id(self):
        # Sends newSession
        self.marionette.start_session(session_id="ILoveCheese")

        self.assertEqual(self.marionette.session_id, "ILoveCheese")
        self.assertTrue(isinstance(self.marionette.session_id, unicode))

    def test_we_only_support_one_active_session_at_a_time(self):
        self.marionette.start_session()
        self.assertTrue(isinstance(self.marionette.session_id, unicode))
        self.assertRaises(SessionNotCreatedException, self.marionette._send_message, "newSession", {})
