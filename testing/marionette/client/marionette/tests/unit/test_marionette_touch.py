# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette_test import MarionetteTestCase
from marionette import Marionette
from marionette_touch import MarionetteTouchMixin   

class TestTouchMixin(MarionetteTestCase):

    def setUp(self):
        super(TestTouchMixin, self).setUp()
        self.marionette.__class__ = type('Marionette', (Marionette, MarionetteTouchMixin), {})
        self.marionette.setup_touch()
        testAction = self.marionette.absolute_url("testAction.html")
        self.marionette.navigate(testAction)

    def tap(self):
        button = self.marionette.find_element("id", "button1")
        self.marionette.tap(button)

    def double_tap(self):
        button = self.marionette.find_element("id", "button1")
        self.marionette.double_tap(button)

    def test_tap(self):
        self.tap()
        expected = "button1-touchstart-touchend-mousedown-mouseup-click"
        self.wait_for_condition(lambda m: m.execute_script("return document.getElementById('button1').innerHTML;") == expected)

    def test_tap_mouse_shim(self):
        self.marionette.execute_script("window.wrappedJSObject.MouseEventShim = 'mock value';")
        self.tap()
        expected = "button1-touchstart-touchend"
        self.wait_for_condition(lambda m: m.execute_script("return document.getElementById('button1').innerHTML;") == expected)

    def test_double_tap(self):
        self.double_tap()
        expected = "button1-touchstart-touchend-mousemove-mousedown-mouseup-click-touchstart-touchend-mousemove-mousedown-mouseup-click"
        self.wait_for_condition(lambda m: m.execute_script("return document.getElementById('button1').innerHTML;") == expected)

    """
    #Enable this when we have logic in double_tap to handle the shim
    def test_double_tap_mouse_shim(self):
        self.marionette.execute_script("window.wrappedJSObject.MouseEventShim = 'mock value';")
        self.double_tap()
        expected = "button1-touchstart-touchend-touchstart-touchend"
        self.wait_for_condition(lambda m: m.execute_script("return document.getElementById('button1').innerHTML;") == expected)
    """

    def test_flick(self):
        button = self.marionette.find_element("id", "button1")
        self.marionette.flick(button, 0, 0, 0, 200)
        expected = "button1-touchstart-touchmove"
        self.wait_for_condition(lambda m: m.execute_script("return document.getElementById('button1').innerHTML;") == expected)
        self.assertEqual("buttonFlick-touchmove-touchend", self.marionette.execute_script("return document.getElementById('buttonFlick').innerHTML;"))
