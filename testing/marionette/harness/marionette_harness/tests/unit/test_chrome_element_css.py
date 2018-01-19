# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

from marionette_driver.by import By

from marionette_harness import MarionetteTestCase


class TestChromeElementCSS(MarionetteTestCase):

    def get_element_computed_style(self, element, property):
        return self.marionette.execute_script("""
            const [el, prop] = arguments;
            const elStyle = window.getComputedStyle(el);
            return elStyle[prop];""",
            script_args=(element, property),
            sandbox=None)

    def test_we_can_get_css_value_on_chrome_element(self):
        with self.marionette.using_context("chrome"):
            identity_icon = self.marionette.find_element(By.ID, "identity-icon")
            favicon_image = identity_icon.value_of_css_property("list-style-image")
            self.assertIn("chrome://", favicon_image)
            identity_box = self.marionette.find_element(By.ID, "identity-box")
            expected_bg_colour = self.get_element_computed_style(
                identity_box, "backgroundColor")
            actual_bg_colour = identity_box.value_of_css_property("background-color")
            self.assertEqual(expected_bg_colour, actual_bg_colour)
