# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette import MarionetteTestCase
from marionette_driver.errors import NoSuchElementException
from marionette_driver.by import By


class TestImplicitWaits(MarionetteTestCase):
    def test_implicitly_wait_for_single_element(self):
        test_html = self.marionette.absolute_url("test_dynamic.html")
        self.marionette.navigate(test_html)
        add = self.marionette.find_element(By.ID, "adder")
        self.marionette.timeout.implicit = 30
        add.click()
        # all is well if this does not throw
        self.marionette.find_element(By.ID, "box0")

    def test_implicit_wait_reaches_timeout(self):
        test_html = self.marionette.absolute_url("test_dynamic.html")
        self.marionette.navigate(test_html)
        self.marionette.timeout.implicit = 3
        with self.assertRaises(NoSuchElementException):
            self.marionette.find_element(By.ID, "box0")
