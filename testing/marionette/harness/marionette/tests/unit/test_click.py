# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette_driver.by import By
from marionette_driver.errors import NoSuchElementException, ElementNotVisibleException
from marionette import MarionetteTestCase
from marionette_driver.marionette import Actions
from marionette_driver.keys import Keys
from marionette_driver.wait import Wait


class TestClick(MarionetteTestCase):
    def test_click(self):
        test_html = self.marionette.absolute_url("test.html")
        self.marionette.navigate(test_html)
        link = self.marionette.find_element(By.ID, "mozLink")
        link.click()
        self.assertEqual("Clicked", self.marionette.execute_script("return document.getElementById('mozLink').innerHTML;"))

    def test_clicking_a_link_made_up_of_numbers_is_handled_correctly(self):
        test_html = self.marionette.absolute_url("clicks.html")
        self.marionette.navigate(test_html)
        self.marionette.find_element(By.LINK_TEXT, "333333").click()
        Wait(self.marionette, timeout=30, ignored_exceptions=NoSuchElementException).until(
            lambda m: m.find_element(By.ID, 'username'))
        self.assertEqual(self.marionette.title, "XHTML Test Page")

    def test_clicking_an_element_that_is_not_displayed_raises(self):
        test_html = self.marionette.absolute_url('hidden.html')
        self.marionette.navigate(test_html)

        with self.assertRaises(ElementNotVisibleException):
            self.marionette.find_element(By.ID, 'child').click()

    def test_clicking_on_a_multiline_link(self):
        test_html = self.marionette.absolute_url("clicks.html")
        self.marionette.navigate(test_html)
        self.marionette.find_element(By.ID, "overflowLink").click()
        self.wait_for_condition(lambda mn: self.marionette.title == "XHTML Test Page")
