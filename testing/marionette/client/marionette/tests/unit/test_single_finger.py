# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import time
from marionette_test import MarionetteTestCase
from marionette import Actions

class testSingleFinger(MarionetteTestCase):
    def test_wait(self):
        testTouch = self.marionette.absolute_url("testAction.html")
        self.marionette.navigate(testTouch)
        button = self.marionette.find_element("id", "mozLinkCopy")
        action = Actions(self.marionette)
        action.press(button).wait(5).release()
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
