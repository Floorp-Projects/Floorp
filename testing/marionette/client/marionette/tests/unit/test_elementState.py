# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette_test import MarionetteTestCase


class TestState(MarionetteTestCase):
    def test_isEnabled(self):
        test_html = self.marionette.absolute_url("test.html")
        self.marionette.navigate(test_html)
        l = self.marionette.find_element("name", "myCheckBox")
        self.assertTrue(l.is_enabled())
        self.marionette.execute_script("arguments[0].disabled = true;", [l])
        self.assertFalse(l.is_enabled())

    def test_isDisplayed(self):
        test_html = self.marionette.absolute_url("test.html")
        self.marionette.navigate(test_html)
        l = self.marionette.find_element("name", "myCheckBox")
        self.assertTrue(l.is_displayed())
        self.marionette.execute_script("arguments[0].hidden = true;", [l])
        self.assertFalse(l.is_displayed())
