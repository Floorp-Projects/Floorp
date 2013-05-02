# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import time
from marionette_test import MarionetteTestCase
from marionette import Marionette
from marionette_touch import MarionetteTouchMixin   

class TestTouchMixin(MarionetteTestCase):

    def setUp(self):
        super(TestTouchMixin, self).setUp()
        self.marionette.__class__ = type('Marionette', (Marionette, MarionetteTouchMixin), {})
        self.marionette.setup_touch()

    def test_tap(self):
        testTouch = self.marionette.absolute_url("testAction.html")
        self.marionette.navigate(testTouch)
        button = self.marionette.find_element("id", "mozLinkCopy")
        self.marionette.tap(button)
        time.sleep(10)
        self.assertEqual("End", self.marionette.execute_script("return document.getElementById('mozLinkCopy').innerHTML;"))

    def test_dbtap(self):
        testTouch = self.marionette.absolute_url("testAction.html")
        self.marionette.navigate(testTouch)
        button = self.marionette.find_element("id", "mozMouse")
        self.marionette.double_tap(button)
        time.sleep(10)
        self.assertEqual("TouchEnd2", self.marionette.execute_script("return document.getElementById('mozMouse').innerHTML;"))

    def test_flick(self):
        testTouch = self.marionette.absolute_url("testAction.html")
        self.marionette.navigate(testTouch)
        button = self.marionette.find_element("id", "mozLinkScrollStart")
        self.marionette.flick(button, 0, 0, 0, -250)
        time.sleep(15)
        self.assertEqual("End", self.marionette.execute_script("return document.getElementById('mozLinkScroll').innerHTML;"))
        self.assertEqual("Start", self.marionette.execute_script("return document.getElementById('mozLinkScrollStart').innerHTML;"))
