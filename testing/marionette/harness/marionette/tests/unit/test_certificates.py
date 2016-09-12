# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette import MarionetteTestCase
from marionette_driver.errors import UnknownException


class TestCertificates(MarionetteTestCase):
    def test_block_insecure_sites(self):
        self.marionette.delete_session()
        self.marionette.start_session()

        self.marionette.navigate(self.fixtures.where_is("test.html", on="http"))
        self.assertIn("http://", self.marionette.get_url())
        with self.assertRaises(UnknownException):
            self.marionette.navigate(self.fixtures.where_is("test.html", on="https"))

    def test_accept_all_insecure(self):
        self.marionette.delete_session()
        self.marionette.start_session({"desiredCapability": {"acceptSslCerts": ["*"]}})
        self.marionette.navigate(self.fixtures.where_is("test.html", on="https"))
        self.assertIn("https://", self.marionette.url)

    """
    def test_accept_some_insecure(self):
        self.marionette.delete_session()
        self.marionette.start_session({"requiredCapabilities": {"acceptSslCerts": ["127.0.0.1"]}})
        self.marionette.navigate(self.fixtures.where_is("test.html", on="https"))
        self.assertIn("https://", self.marionette.url)
    """