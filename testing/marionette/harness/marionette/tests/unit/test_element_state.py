# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette import MarionetteTestCase
from marionette_driver.by import By


class TestIsElementEnabled(MarionetteTestCase):
    def test_is_enabled(self):
        test_html = self.marionette.absolute_url("test.html")
        self.marionette.navigate(test_html)
        l = self.marionette.find_element(By.NAME, "myCheckBox")
        self.assertTrue(l.is_enabled())
        self.marionette.execute_script("arguments[0].disabled = true;", [l])
        self.assertFalse(l.is_enabled())


class TestIsElementDisplayed(MarionetteTestCase):
    def test_is_displayed(self):
        test_html = self.marionette.absolute_url("test.html")
        self.marionette.navigate(test_html)
        l = self.marionette.find_element(By.NAME, "myCheckBox")
        self.assertTrue(l.is_displayed())
        self.marionette.execute_script("arguments[0].hidden = true;", [l])
        self.assertFalse(l.is_displayed())


class TestGetElementAttribute(MarionetteTestCase):
    def test_get(self):
        test_html = self.marionette.absolute_url("test.html")
        self.marionette.navigate(test_html)
        l = self.marionette.find_element(By.ID, "mozLink")
        self.assertEqual("mozLink", l.get_attribute("id"))

    def test_boolean(self):
        test_html = self.marionette.absolute_url("html5/boolean_attributes.html")
        self.marionette.navigate(test_html)
        disabled = self.marionette.find_element(By.ID, "disabled")
        self.assertEqual('true', disabled.get_attribute("disabled"))
