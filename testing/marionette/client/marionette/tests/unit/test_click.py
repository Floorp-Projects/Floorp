# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import time
from marionette_test import MarionetteTestCase
from by import By


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
        count = 0
        while len(self.marionette.find_elements(By.ID, "username")) == 0:
            count += 1
            time.sleep(1)
            if count == 30:
                self.fail("Element id=username not found after 30 seconds")

        self.assertEqual(self.marionette.title, "XHTML Test Page")
