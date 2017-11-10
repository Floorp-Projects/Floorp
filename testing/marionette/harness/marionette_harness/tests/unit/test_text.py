# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette_driver.by import By
from marionette_harness import MarionetteTestCase


class TestText(MarionetteTestCase):

    def test_get_text(self):
        test_html = self.marionette.absolute_url("test.html")
        self.marionette.navigate(test_html)
        l = self.marionette.find_element(By.ID, "mozLink")
        self.assertEqual("Click me!", l.text)

    def test_clear_text(self):
        test_html = self.marionette.absolute_url("test.html")
        self.marionette.navigate(test_html)
        l = self.marionette.find_element(By.NAME, "myInput")
        self.assertEqual("asdf", self.marionette.execute_script("return arguments[0].value;", [l]))
        l.clear()
        self.assertEqual("", self.marionette.execute_script("return arguments[0].value;", [l]))
