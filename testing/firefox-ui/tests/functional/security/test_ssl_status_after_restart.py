# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette_driver import By, Wait
from marionette_harness import MarionetteTestCase, WindowManagerMixin


class TestSSLStatusAfterRestart(WindowManagerMixin, MarionetteTestCase):
    def setUp(self):
        super(TestSSLStatusAfterRestart, self).setUp()
        self.marionette.set_context("chrome")

        self.test_url = "https://www.itisatrap.org/"

        # Set browser to restore previous session
        self.marionette.set_pref("browser.startup.page", 3)
        # Disable rcwn to make cache behavior deterministic
        self.marionette.set_pref("network.http.rcwn.enable", False)

    def tearDown(self):
        self.marionette.clear_pref("browser.startup.page")
        self.marionette.clear_pref("network.http.rcwn.enable")

        super(TestSSLStatusAfterRestart, self).tearDown()

    def test_ssl_status_after_restart(self):
        with self.marionette.using_context("content"):
            self.marionette.navigate(self.test_url)
        self.verify_certificate_status(self.test_url)

        self.marionette.restart(in_app=True)

        self.verify_certificate_status(self.test_url)

    def verify_certificate_status(self, url):
        with self.marionette.using_context("content"):
            Wait(self.marionette).until(
                lambda _: self.marionette.get_url() == url,
                message="Expected URL loaded",
            )

        identity_box = self.marionette.find_element(By.ID, "identity-box")
        self.assertEqual(identity_box.get_attribute("pageproxystate"), "valid")

        class_list = self.marionette.execute_script(
            """
          const names = [];
          for (const name of arguments[0].classList) {
            names.push(name);
          }
          return names;
        """,
            script_args=[identity_box],
        )
        self.assertIn("verifiedDomain", class_list)
