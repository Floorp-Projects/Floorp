# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette import MarionetteTestCase


class TestChromeElementCSS(MarionetteTestCase):

    def test_we_can_get_css_value_on_chrome_element(self):
        self.marionette.navigate("about:blank")
        with self.marionette.using_context("chrome"):
            element = self.marionette.find_element("id", "page-proxy-favicon")
            favicon_image = element.value_of_css_property("list-style-image")

            self.assertIn("identity-not-secure.svg", favicon_image)

            element = self.marionette.find_element("id", "identity-box")
            background_colour = element.value_of_css_property("background-color")

            self.assertEqual("transparent", background_colour)
