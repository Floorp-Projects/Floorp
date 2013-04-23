# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import time
from marionette_test import MarionetteTestCase
from marionette import Actions
from errors import NoSuchElementException, MarionetteException

class testSingleFinger(MarionetteTestCase):
    def test_wait(self):
        testTouch = self.marionette.absolute_url("testAction.html")
        self.marionette.navigate(testTouch)
        button = self.marionette.find_element("id", "mozLinkCopy")
        action = Actions(self.marionette)
        action.press(button).wait(0.2).release()
        action.perform()
        time.sleep(15)
        self.assertEqual("End", self.marionette.execute_script("return document.getElementById('mozLinkCopy').innerHTML;"))

    def test_move_element(self):
        testTouch = self.marionette.absolute_url("testAction.html")
        self.marionette.navigate(testTouch)
        ele = self.marionette.find_element("id", "mozLink")
        drop = self.marionette.find_element("id", "mozLinkPos")
        action = Actions(self.marionette)
        action.press(ele).move(drop).release()
        action.perform()
        time.sleep(15)
        self.assertEqual("Move", self.marionette.execute_script("return document.getElementById('mozLink').innerHTML;"))
        self.assertEqual("End", self.marionette.execute_script("return document.getElementById('mozLinkPos').innerHTML;"))

    def test_move_by_offset(self):
        testTouch = self.marionette.absolute_url("testAction.html")
        self.marionette.navigate(testTouch)
        ele = self.marionette.find_element("id", "mozLink")
        action = Actions(self.marionette)
        action.press(ele).move_by_offset(0,150).move_by_offset(0,150).release()
        action.perform()
        time.sleep(15)
        self.assertEqual("Move", self.marionette.execute_script("return document.getElementById('mozLink').innerHTML;"))
        self.assertEqual("End", self.marionette.execute_script("return document.getElementById('mozLinkPos').innerHTML;"))

    def test_chain(self):
        testTouch = self.marionette.absolute_url("testAction.html")
        self.marionette.navigate(testTouch)
        action = Actions(self.marionette)
        button1 = self.marionette.find_element("id", "mozLinkCopy2")
        action.press(button1).perform()
        button2 = self.marionette.find_element("id", "delayed")
        time.sleep(5)
        action.move(button2).release().perform()
        time.sleep(15)
        self.assertEqual("End", self.marionette.execute_script("return document.getElementById('delayed').innerHTML;"))

    def test_no_press(self):
        testTouch = self.marionette.absolute_url("testAction.html")
        self.marionette.navigate(testTouch)
        action = Actions(self.marionette)
        action.release()
        self.assertRaises(NoSuchElementException, action.perform)

    def test_mouse_wait(self):
        testTouch = self.marionette.absolute_url("testAction.html")
        self.marionette.navigate(testTouch)
        self.marionette.send_mouse_event(True)
        action = Actions(self.marionette)
        button = self.marionette.find_element("id", "mozMouse")
        action.press(button).wait().release().perform()
        time.sleep(15)
        self.assertEqual("MouseClick", self.marionette.execute_script("return document.getElementById('mozMouse').innerHTML;"))

    def test_mouse_wait_more(self):
        testTouch = self.marionette.absolute_url("testAction.html")
        self.marionette.navigate(testTouch)
        self.marionette.send_mouse_event(True)
        action = Actions(self.marionette)
        button = self.marionette.find_element("id", "mozMouse")
        action.press(button).wait(0.1).release().perform()
        time.sleep(15)
        self.assertEqual("MouseClick", self.marionette.execute_script("return document.getElementById('mozMouse').innerHTML;"))

    def test_mouse_no_wait(self):
        testTouch = self.marionette.absolute_url("testAction.html")
        self.marionette.navigate(testTouch)
        self.marionette.send_mouse_event(True)
        action = Actions(self.marionette)
        button = self.marionette.find_element("id", "mozMouse")
        action.press(button).release().perform()
        time.sleep(15)
        self.assertEqual("MouseClick", self.marionette.execute_script("return document.getElementById('mozMouse').innerHTML;"))

    def test_no_mouse_wait(self):
        testTouch = self.marionette.absolute_url("testAction.html")
        self.marionette.navigate(testTouch)
        self.marionette.send_mouse_event(False)
        action = Actions(self.marionette)
        button = self.marionette.find_element("id", "mozMouse")
        action.press(button).wait().release().perform()
        time.sleep(15)
        self.assertEqual("TouchEnd", self.marionette.execute_script("return document.getElementById('mozMouse').innerHTML;"))

    def test_no_mouse_no_wait(self):
        testTouch = self.marionette.absolute_url("testAction.html")
        self.marionette.navigate(testTouch)
        self.marionette.send_mouse_event(False)
        action = Actions(self.marionette)
        button = self.marionette.find_element("id", "mozMouse")
        action.press(button).release().perform()
        time.sleep(15)
        self.assertEqual("TouchEnd", self.marionette.execute_script("return document.getElementById('mozMouse').innerHTML;"))

    def test_long_press(self):
        testTouch = self.marionette.absolute_url("testAction.html")
        self.marionette.navigate(testTouch)
        button = self.marionette.find_element("id", "mozLinkCopy")
        action = Actions(self.marionette)
        action.press(button).wait(5).perform()
        time.sleep(10)
        self.assertEqual("Context", self.marionette.execute_script("return document.getElementById('mozLinkCopy').innerHTML;"))
        action.release().perform()
        time.sleep(10)
        self.assertEqual("ContextEnd", self.marionette.execute_script("return document.getElementById('mozLinkCopy').innerHTML;"))

    def test_long_press_action(self):
        testTouch = self.marionette.absolute_url("testAction.html")
        self.marionette.navigate(testTouch)
        button = self.marionette.find_element("id", "mozLinkCopy")
        action = Actions(self.marionette)
        action.long_press(button, 5).perform()
        time.sleep(10)
        self.assertEqual("ContextEnd", self.marionette.execute_script("return document.getElementById('mozLinkCopy').innerHTML;"))

    def test_long_press_fail(self):
        testTouch = self.marionette.absolute_url("testAction.html")
        self.marionette.navigate(testTouch)
        button = self.marionette.find_element("id", "mozLinkCopy")
        action = Actions(self.marionette)
        action.press(button).long_press(button, 5)
        self.assertRaises(MarionetteException, action.perform)

    def test_wrong_value(self):
        testTouch = self.marionette.absolute_url("testAction.html")
        self.marionette.navigate(testTouch)
        self.assertRaises(MarionetteException, self.marionette.send_mouse_event, "boolean")

