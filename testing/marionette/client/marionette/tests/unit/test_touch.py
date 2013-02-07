# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import time
from marionette_test import MarionetteTestCase
from marionette import HTMLElement
from errors import MarionetteException

class testTouch(MarionetteTestCase):
    def test_touch(self):
      testTouch = self.marionette.absolute_url("testTouch.html")
      self.marionette.navigate(testTouch)
      button = self.marionette.find_element("id", "mozLink")
      button.single_tap(0, 300)
      time.sleep(10)
      self.assertEqual("Clicked", self.marionette.execute_script("return document.getElementById('mozLinkPos').innerHTML;"))
      button.single_tap()
      time.sleep(10)
      self.assertEqual("Clicked", self.marionette.execute_script("return document.getElementById('mozLink').innerHTML;"))

    def test_invisible(self):
      testTouch = self.marionette.absolute_url("testTouch.html")
      self.marionette.navigate(testTouch)
      ele = self.marionette.find_element("id", "testh2")
      self.assertRaises(MarionetteException, ele.single_tap)

    def test_scrolling(self):
      testTouch = self.marionette.absolute_url("testTouch.html")
      self.marionette.navigate(testTouch)
      ele = self.marionette.find_element("id", "scroll")
      ele.single_tap()
      time.sleep(10)
      self.assertEqual("Clicked", self.marionette.execute_script("return document.getElementById('scroll').innerHTML;"))
