# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import time
from marionette_test import MarionetteTestCase
from marionette import MultiActions, Actions

class testSingleFinger(MarionetteTestCase):
    def test_move_element(self):
      testTouch = self.marionette.absolute_url("testAction.html")
      self.marionette.navigate(testTouch)
      start = self.marionette.find_element("id", "mozLink")
      drop = self.marionette.find_element("id", "mozLinkPos")
      ele = self.marionette.find_element("id", "mozLinkCopy")
      multi_action = MultiActions(self.marionette)
      action1 = Actions(self.marionette)
      action2 = Actions(self.marionette)
      action1.press(start).move(drop).wait(3).release()
      action2.press(ele).wait().release()
      multi_action.add(action1).add(action2).perform()
      time.sleep(15)
      self.assertEqual("Move", self.marionette.execute_script("return document.getElementById('mozLink').innerHTML;"))
      self.assertEqual("End", self.marionette.execute_script("return document.getElementById('mozLinkPos').innerHTML;"))
      self.assertEqual("End", self.marionette.execute_script("return document.getElementById('mozLinkCopy').innerHTML;"))

    def test_move_offset_element(self):
      testTouch = self.marionette.absolute_url("testAction.html")
      self.marionette.navigate(testTouch)
      start = self.marionette.find_element("id", "mozLink")
      ele = self.marionette.find_element("id", "mozLinkCopy")
      multi_action = MultiActions(self.marionette)
      action1 = Actions(self.marionette)
      action2 = Actions(self.marionette)
      action1.press(start).move_by_offset(0,300).wait().release()
      action2.press(ele).wait(5).release()
      multi_action.add(action1).add(action2).perform()
      time.sleep(15)
      self.assertEqual("Move", self.marionette.execute_script("return document.getElementById('mozLink').innerHTML;"))
      self.assertEqual("End", self.marionette.execute_script("return document.getElementById('mozLinkPos').innerHTML;"))
      self.assertEqual("End", self.marionette.execute_script("return document.getElementById('mozLinkCopy').innerHTML;"))

    def test_three_fingers(self):
      testTouch = self.marionette.absolute_url("testAction.html")
      self.marionette.navigate(testTouch)
      start_one = self.marionette.find_element("id", "mozLink")
      start_two = self.marionette.find_element("id", "mozLinkStart")
      drop_two = self.marionette.find_element("id", "mozLinkEnd")
      ele = self.marionette.find_element("id", "mozLinkCopy2")
      multi_action = MultiActions(self.marionette)
      action1 = Actions(self.marionette)
      action2 = Actions(self.marionette)
      action3 = Actions(self.marionette)
      action1.press(start_one).move_by_offset(0,300).release()
      action2.press(ele).wait().wait(5).release()
      action3.press(start_two).move(drop_two).wait(2).release()
      multi_action.add(action1).add(action2).add(action3).perform()
      time.sleep(15)
      self.assertEqual("Move", self.marionette.execute_script("return document.getElementById('mozLink').innerHTML;"))
      self.assertEqual("End", self.marionette.execute_script("return document.getElementById('mozLinkPos').innerHTML;"))
      self.assertTrue(self.marionette.execute_script("return document.getElementById('mozLinkCopy2').innerHTML >= 5000;"))
      self.assertTrue(self.marionette.execute_script("return document.getElementById('mozLinkEnd').innerHTML >= 5000;"))
