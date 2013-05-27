# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette_test import MarionetteTestCase
from marionette import Actions
from errors import MarionetteException
#add this directory to the path
import os
import sys
sys.path.append(os.path.dirname(__file__))
from single_finger_functions import *

class testSingleFinger(MarionetteTestCase):
    def test_press_release(self):
        press_release(self.marionette, self.wait_for_condition, "button1-touchstart-touchend-mousemove-mousedown-mouseup-click")

    def test_move_element(self):
        move_element(self.marionette, self.wait_for_condition, "button1-touchstart", "button2-touchmove-touchend")

    """
    #Skipping due to Bug 874914
    def test_move_by_offset(self):
        move_element_offset(self.marionette, self.wait_for_condition, "button1-touchstart", "button2-touchmove-touchend")
    """

    def test_no_press(self):
        testAction = self.marionette.absolute_url("testAction.html")
        self.marionette.navigate(testAction)
        action = Actions(self.marionette)
        action.release()
        self.assertRaises(MarionetteException, action.perform)

    def test_wait(self):
        wait(self.marionette, self.wait_for_condition, "button1-touchstart-touchend-mousemove-mousedown-mouseup-click")

    def test_wait_with_value(self):
        wait_with_value(self.marionette, self.wait_for_condition, "button1-touchstart-touchend-mousemove-mousedown-mouseup-click")

    def test_context_menu(self):
        context_menu(self.marionette, self.wait_for_condition, "button1-touchstart-contextmenu", "button1-touchstart-contextmenu-touchend")

    def test_long_press_action(self):
        long_press_action(self.marionette, self.wait_for_condition, "button1-touchstart-contextmenu-touchend")

    """
    #Skipping due to Bug 865334
    def test_long_press_fail(self):
        testAction = self.marionette.absolute_url("testAction.html")
        self.marionette.navigate(testAction)
        button = self.marionette.find_element("id", "button1Copy")
        action = Actions(self.marionette)
        action.press(button).long_press(button, 5)
        self.assertRaises(MarionetteException, action.perform)
    """

    def test_chain(self):
        chain(self.marionette, self.wait_for_condition, "button1-touchstart", "delayed-touchmove-touchend")

    """
    #Skipping due to Bug 874914. Flick uses chained moveByOffset calls
    def test_chain_flick(self):
        chain_flick(self.marionette, self.wait_for_condition, "button1-touchstart-touchmove", "buttonFlick-touchmove-touchend")
    """

    """
    #Skipping due to Bug 865334
    def test_touchcancel_chain(self):
        testAction = self.marionette.absolute_url("testAction.html")
        self.marionette.navigate(testAction)
        button = self.marionette.find_element("id", "button1")
        action = Actions(self.marionette)
        action.press(button).wait(5).cancel()
        action.perform()
        expected = "button1-touchstart-touchcancel"
        self.wait_for_condition(lambda m: m.execute_script("return document.getElementById('button1').innerHTML;") == expected)
    """

    def test_single_tap(self):
        single_tap(self.marionette, self.wait_for_condition, "button1-touchstart-touchend-mousemove-mousedown-mouseup-click")

    def test_double_tap(self):
        double_tap(self.marionette, self.wait_for_condition, "button1-touchstart-touchend-mousemove-mousedown-mouseup-click-touchstart-touchend-mousemove-mousedown-mouseup-click")

