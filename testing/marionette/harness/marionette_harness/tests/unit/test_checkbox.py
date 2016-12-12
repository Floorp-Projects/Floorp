# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette_driver.by import By

from marionette_harness import MarionetteTestCase


class TestCheckbox(MarionetteTestCase):
    def test_selected(self):
        test_html = self.marionette.absolute_url("test.html")
        self.marionette.navigate(test_html)
        box = self.marionette.find_element(By.NAME, "myCheckBox")
        self.assertFalse(box.is_selected())
        box.click()
        self.assertTrue(box.is_selected())
