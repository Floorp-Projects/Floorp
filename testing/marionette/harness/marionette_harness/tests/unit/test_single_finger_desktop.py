# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import sys

from marionette_driver.errors import MarionetteException
from marionette_driver.by import By

from marionette_harness import MarionetteTestCase

# add this directory to the path
sys.path.append(os.path.dirname(__file__))

from single_finger_functions import (
    chain, chain_flick, context_menu, double_tap,
    long_press_action, long_press_on_xy_action,
    move_element, move_element_offset, press_release, single_tap, wait,
    wait_with_value
)


class testSingleFingerMouse(MarionetteTestCase):
    def setUp(self):
        super(MarionetteTestCase, self).setUp()
        # set context menu related preferences needed for some tests
        self.marionette.set_context("chrome")
        self.enabled = self.marionette.execute_script("""
let prefs = Components.classes["@mozilla.org/preferences-service;1"]
                              .getService(Components.interfaces.nsIPrefBranch);
let value = false;
try {
  value = prefs.getBoolPref("ui.click_hold_context_menus");
}
catch (e) {}
prefs.setBoolPref("ui.click_hold_context_menus", true);
return value;
""")
        self.wait_time = self.marionette.execute_script("""
let prefs = Components.classes["@mozilla.org/preferences-service;1"]
                              .getService(Components.interfaces.nsIPrefBranch);
let value = 750;
try {
  value = prefs.getIntPref("ui.click_hold_context_menus.delay");
}
catch (e) {}
prefs.setIntPref("ui.click_hold_context_menus.delay", value);
return value;
""")
        self.marionette.set_context("content")

    def tearDown(self):
        self.marionette.set_context("chrome")
        self.marionette.execute_script(
                          """
let prefs = Components.classes["@mozilla.org/preferences-service;1"]
                              .getService(Components.interfaces.nsIPrefBranch);
prefs.setBoolPref("ui.click_hold_context_menus", arguments[0]);
""", [self.enabled])
        self.marionette.execute_script(
                          """
let prefs = Components.classes["@mozilla.org/preferences-service;1"]
                              .getService(Components.interfaces.nsIPrefBranch);
prefs.setIntPref("ui.click_hold_context_menus.delay", arguments[0]);
""", [self.wait_time])
        self.marionette.set_context("content")
        super(MarionetteTestCase, self).tearDown()

    def test_press_release(self):
        press_release(self.marionette, 1, self.wait_for_condition, "button1-mousemove-mousedown-mouseup-click")

    def test_press_release_twice(self):
        press_release(self.marionette, 2, self.wait_for_condition, "button1-mousemove-mousedown-mouseup-click-mousemove-mousedown-mouseup-click")

    def test_move_element(self):
        move_element(self.marionette, self.wait_for_condition, "button1-mousemove-mousedown", "button2-mousemove-mouseup")

    def test_move_by_offset(self):
        move_element_offset(self.marionette, self.wait_for_condition, "button1-mousemove-mousedown", "button2-mousemove-mouseup")

    def test_wait(self):
        wait(self.marionette, self.wait_for_condition, "button1-mousemove-mousedown-mouseup-click")

    def test_wait_with_value(self):
        wait_with_value(self.marionette, self.wait_for_condition, "button1-mousemove-mousedown-mouseup-click")

    """
    // Skipping due to Bug 1191066
    def test_context_menu(self):
        context_menu(self.marionette, self.wait_for_condition, "button1-mousemove-mousedown-contextmenu", "button1-mousemove-mousedown-contextmenu-mouseup-click")

    def test_long_press_action(self):
        long_press_action(self.marionette, self.wait_for_condition, "button1-mousemove-mousedown-contextmenu-mouseup-click")

    def test_long_press_on_xy_action(self):
        long_press_on_xy_action(self.marionette, self.wait_for_condition, "button1-mousemove-mousedown-contextmenu-mouseup-click")
    """

    """
    //Skipping due to Bug 865334
    def test_long_press_fail(self):
        testAction = self.marionette.absolute_url("testAction.html")
        self.marionette.navigate(testAction)
        button = self.marionette.find_element(By.ID, "button1Copy")
        action = Actions(self.marionette)
        action.press(button).long_press(button, 5)
        assertRaises(MarionetteException, action.perform)
    """

    def test_chain(self):
        chain(self.marionette, self.wait_for_condition, "button1-mousemove-mousedown", "delayed-mousemove-mouseup")

    def test_chain_flick(self):
        chain_flick(self.marionette, self.wait_for_condition, "button1-mousemove-mousedown-mousemove", "buttonFlick-mousemove-mouseup")

    def test_single_tap(self):
        single_tap(self.marionette, self.wait_for_condition, "button1-mousemove-mousedown-mouseup-click")

    def test_double_tap(self):
        double_tap(self.marionette, self.wait_for_condition, "button1-mousemove-mousedown-mouseup-click-mousemove-mousedown-mouseup-click")
