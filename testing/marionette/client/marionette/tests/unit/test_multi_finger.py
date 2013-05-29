# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette_test import MarionetteTestCase
from marionette import MultiActions, Actions

class testMultiFinger(MarionetteTestCase):
    def test_move_element(self):
      testAction = self.marionette.absolute_url("testAction.html")
      self.marionette.navigate(testAction)
      start = self.marionette.find_element("id", "button1")
      drop = self.marionette.find_element("id", "button2")
      ele = self.marionette.find_element("id", "button3")
      multi_action = MultiActions(self.marionette)
      action1 = Actions(self.marionette)
      action2 = Actions(self.marionette)
      action1.press(start).move(drop).wait(3).release()
      action2.press(ele).wait().release()
      multi_action.add(action1).add(action2).perform()
      expected = "button1-touchstart"
      self.wait_for_condition(lambda m: m.execute_script("return document.getElementById('button1').innerHTML;") == expected)
      self.assertEqual("button2-touchmove-touchend", self.marionette.execute_script("return document.getElementById('button2').innerHTML;"))
      self.assertTrue("button3-touchstart-touchend" in self.marionette.execute_script("return document.getElementById('button3').innerHTML;"))

    def test_move_offset_element(self):
      testAction = self.marionette.absolute_url("testAction.html")
      self.marionette.navigate(testAction)
      start = self.marionette.find_element("id", "button1")
      ele = self.marionette.find_element("id", "button3")
      multi_action = MultiActions(self.marionette)
      action1 = Actions(self.marionette)
      action2 = Actions(self.marionette)
      action1.press(start).move_by_offset(0,300).wait().release()
      action2.press(ele).wait(5).release()
      multi_action.add(action1).add(action2).perform()
      expected = "button1-touchstart"
      self.wait_for_condition(lambda m: m.execute_script("return document.getElementById('button1').innerHTML;") == expected)
      self.assertEqual("button2-touchmove-touchend", self.marionette.execute_script("return document.getElementById('button2').innerHTML;"))
      self.assertTrue("button3-touchstart-touchend" in self.marionette.execute_script("return document.getElementById('button3').innerHTML;"))

    def test_three_fingers(self):
      testAction = self.marionette.absolute_url("testAction.html")
      self.marionette.navigate(testAction)
      start_one = self.marionette.find_element("id", "button1")
      start_two = self.marionette.find_element("id", "button2")
      element1 = self.marionette.find_element("id", "button3")
      element2 = self.marionette.find_element("id", "button4")
      multi_action = MultiActions(self.marionette)
      action1 = Actions(self.marionette)
      action2 = Actions(self.marionette)
      action3 = Actions(self.marionette)
      action1.press(start_one).move_by_offset(0,300).release()
      action2.press(element1).wait().wait(5).release()
      action3.press(element2).wait().wait().release()
      multi_action.add(action1).add(action2).add(action3).perform()
      expected = "button1-touchstart"
      self.wait_for_condition(lambda m: m.execute_script("return document.getElementById('button1').innerHTML;") == expected)
      self.assertEqual("button2-touchmove-touchend", self.marionette.execute_script("return document.getElementById('button2').innerHTML;"))
      button3_text = self.marionette.execute_script("return document.getElementById('button3').innerHTML;")
      button4_text = self.marionette.execute_script("return document.getElementById('button4').innerHTML;")
      self.assertTrue("button3-touchstart-touchend" in button3_text)
      self.assertTrue("button4-touchstart-touchend" in button4_text)
      self.assertTrue(int(button3_text.rsplit("-")[-1]) >= 5000)
      self.assertTrue(int(button4_text.rsplit("-")[-1]) >= 5000)
