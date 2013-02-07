# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import time
from marionette_test import MarionetteTestCase

class testPressRelease(MarionetteTestCase):
    def test_coordinates(self):
        testTouch = self.marionette.absolute_url("testTouch.html")
        self.marionette.navigate(testTouch)
        button = self.marionette.find_element("id", "mozLink")
        new_id = button.press(0, 300)
        button.release(new_id, 0, 300)
        time.sleep(10)
        self.assertEqual("Clicked", self.marionette.execute_script("return document.getElementById('mozLinkPos').innerHTML;"))
        new_id2 = button.press(0, 0)
        button.release(new_id2, 0, 0)
        time.sleep(10)
        self.assertEqual("Clicked", self.marionette.execute_script("return document.getElementById('mozLink').innerHTML;"))

    def test_no_coordinates(self):
      testTouch = self.marionette.absolute_url("testTouch.html")
      self.marionette.navigate(testTouch)
      ele = self.marionette.find_element("id", "scroll")
      scroll_id = ele.press()
      ele.release(scroll_id)
      time.sleep(10)
      self.assertEqual("Clicked", self.marionette.execute_script("return document.getElementById('scroll').innerHTML;"))
