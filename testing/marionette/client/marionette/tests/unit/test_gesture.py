# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette_test import MarionetteTestCase
from gestures import smooth_scroll, pinch

class testGestures(MarionetteTestCase):
    check_in_viewport = """
        function elementInViewport(el) {
          let rect = el.getBoundingClientRect();
          return (rect.top >= window.pageYOffset &&
                 rect.left >= window.pageXOffset &&
                 rect.bottom <= (window.pageYOffset + window.innerHeight) &&
                 rect.right <= (window.pageXOffset + window.innerWidth)
         );   
        };
    """
    def test_smooth_scroll(self):
        testAction = self.marionette.absolute_url("testAction.html")
        self.marionette.navigate(testAction)
        button = self.marionette.find_element("id", "button2")
        self.assertFalse(self.marionette.execute_script("%s; return elementInViewport(document.getElementById('buttonScroll'));" % self.check_in_viewport))
        smooth_scroll(self.marionette, button, "y",  -1, 800)
        buttonScroll = self.marionette.find_element("id", "buttonScroll")
        self.wait_for_condition(lambda m: m.execute_script("%s; return elementInViewport(arguments[0]);" % self.check_in_viewport, [buttonScroll]) == True)
        self.assertEqual("button2-touchstart", self.marionette.execute_script("return document.getElementById('button2').innerHTML;"))

    """
    #This test doesn't manipulate the page, filed Bug 870377 about it.
    def test_pinch(self):
        testAction = self.marionette.absolute_url("testAction.html")
        self.marionette.navigate(testAction)
        button = self.marionette.find_element("id", "button1")
        pinch(self.marionette, button, 0, 0, 0, 0, 0, -50, 0, 50)
    """
