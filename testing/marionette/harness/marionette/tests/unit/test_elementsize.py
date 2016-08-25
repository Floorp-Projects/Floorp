# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette import MarionetteTestCase
from marionette_driver.by import By


class TestElementSize(MarionetteTestCase):
    def testShouldReturnTheSizeOfALink(self):
        test_html = self.marionette.absolute_url("testSize.html")
        self.marionette.navigate(test_html)
        shrinko = self.marionette.find_element(By.ID, 'linkId')
        size = shrinko.rect
        self.assertTrue(size['width'] > 0)
        self.assertTrue(size['height'] > 0)
