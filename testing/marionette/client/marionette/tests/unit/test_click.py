# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from by import By
from errors import NoSuchElementException
from marionette_test import MarionetteTestCase
from wait import Wait


class TestClick(MarionetteTestCase):
    def test_click(self):
        test_html = self.marionette.absolute_url("test.html")
        self.marionette.navigate(test_html)
        link = self.marionette.find_element(By.ID, "mozLink")
        link.click()
        self.assertEqual("Clicked", self.marionette.execute_script("return document.getElementById('mozLink').innerHTML;"))

    def testClickingALinkMadeUpOfNumbersIsHandledCorrectly(self):
        test_html = self.marionette.absolute_url("clicks.html")
        self.marionette.navigate(test_html)
        self.marionette.find_element(By.LINK_TEXT, "333333").click()
        Wait(self.marionette, timeout=30, ignored_exceptions=NoSuchElementException).until(
            lambda m: m.find_element(By.ID, 'username'))
        self.assertEqual(self.marionette.title, "XHTML Test Page")
