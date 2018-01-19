# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

from marionette_driver import errors

from marionette_harness import MarionetteTestCase


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
        self.assertIn("browserVersion", caps)
        self.assertIn("platformName", caps)
        self.assertIn("platformVersion", caps)

        # Optional capabilities we want Marionette to support
        self.assertIn("rotatable", caps)

    def test_get_session_id(self):
        # Sends newSession
        self.marionette.start_session()

        self.assertTrue(self.marionette.session_id is not None)
        self.assertTrue(isinstance(self.marionette.session_id, unicode))

    def test_session_already_started(self):
        self.marionette.start_session()
        self.assertTrue(isinstance(self.marionette.session_id, unicode))
        with self.assertRaises(errors.SessionNotCreatedException):
            self.marionette._send_message("newSession", {})

    def test_no_session(self):
        with self.assertRaisesRegexp(errors.MarionetteException, "Please start a session"):
            self.marionette.get_url()

        self.marionette.start_session()
        self.marionette.get_url()
