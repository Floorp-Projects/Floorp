# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette import MarionetteTestCase
from marionette_driver.errors import MarionetteException
from marionette_driver.by import By


class testElementTouch(MarionetteTestCase):
    def test_touch(self):
      testAction = self.marionette.absolute_url("testAction.html")
      self.marionette.navigate(testAction)
      button = self.marionette.find_element(By.ID, "button1")
      button.tap()
      expected = "button1-touchstart-touchend-mousemove-mousedown-mouseup-click"
      self.wait_for_condition(lambda m: m.execute_script("return document.getElementById('button1').innerHTML;") == expected)
      button = self.marionette.find_element(By.ID, "button2")
      button.tap()
      expected = "button2-touchstart-touchend-mousemove-mousedown-mouseup-click"
      self.wait_for_condition(lambda m: m.execute_script("return document.getElementById('button2').innerHTML;") == expected)

    def test_invisible(self):
      testAction = self.marionette.absolute_url("testAction.html")
      self.marionette.navigate(testAction)
      ele = self.marionette.find_element(By.ID, "hidden")
      self.assertRaises(MarionetteException, ele.tap)

    def test_scrolling(self):
      testAction = self.marionette.absolute_url("testAction.html")
      self.marionette.navigate(testAction)
      ele = self.marionette.find_element(By.ID, "buttonScroll")
      ele.tap()
      expected = "buttonScroll-touchstart-touchend-mousemove-mousedown-mouseup-click"
      self.wait_for_condition(lambda m: m.execute_script("return document.getElementById('buttonScroll').innerHTML;") == expected)
