# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import time
from marionette_test import MarionetteTestCase
from marionette import HTMLElement

class testSingleFinger(MarionetteTestCase):
    def test_mouse_single_tap(self):
        testTouch = self.marionette.absolute_url("testAction.html")
        self.marionette.navigate(testTouch)
        self.marionette.send_mouse_event(True)
        button = self.marionette.find_element("id", "mozMouse")
        button.tap()
        time.sleep(15)
        self.assertEqual("MouseClick", self.marionette.execute_script("return document.getElementById('mozMouse').innerHTML;"))

    def test_touch(self):
        testTouch = self.marionette.absolute_url("testAction.html")
        self.marionette.navigate(testTouch)
        self.marionette.send_mouse_event(False)
        button = self.marionette.find_element("id", "mozMouse")
        button.tap()
        time.sleep(10)
        self.assertEqual("TouchEnd", self.marionette.execute_script("return document.getElementById('mozMouse').innerHTML;"))
