# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import time
from marionette_test import MarionetteTestCase
from gestures import smooth_scroll, pinch

class testGestures(MarionetteTestCase):
    def test_smooth_scroll(self):
        testTouch = self.marionette.absolute_url("testAction.html")
        self.marionette.navigate(testTouch)
        button = self.marionette.find_element("id", "mozLinkScrollStart")
        smooth_scroll(self.marionette, button, "y",  -1, 250)
        time.sleep(15)
        self.assertEqual("End", self.marionette.execute_script("return document.getElementById('mozLinkScroll').innerHTML;"))

    def test_pinch(self):
        testTouch = self.marionette.absolute_url("testAction.html")
        self.marionette.navigate(testTouch)
        button = self.marionette.find_element("id", "mozLinkScrollStart")
        pinch(self.marionette, button, 0, 0, 0, 0, 0, -50, 0, 50)
        time.sleep(15)
