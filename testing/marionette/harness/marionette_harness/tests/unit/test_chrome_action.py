# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

from marionette_driver import By, errors
from marionette_driver.keys import Keys

from marionette_harness import MarionetteTestCase, WindowManagerMixin


class TestPointerActions(WindowManagerMixin, MarionetteTestCase):
    def setUp(self):
        super(TestPointerActions, self).setUp()

        self.actors_enabled = self.marionette.get_pref("marionette.actors.enabled")
        if self.actors_enabled is None:
            self.actors_enabled = self.marionette.get_pref("fission.autostart")

        self.mouse_chain = self.marionette.actions.sequence(
            "pointer", "pointer_id", {"pointerType": "mouse"}
        )
        self.key_chain = self.marionette.actions.sequence("key", "keyboard_id")

        if self.marionette.session_capabilities["platformName"] == "mac":
            self.mod_key = Keys.META
        else:
            self.mod_key = Keys.CONTROL

        self.marionette.set_context("chrome")

        self.win = self.open_chrome_window("chrome://marionette/content/test.xhtml")
        self.marionette.switch_to_window(self.win)

    def tearDown(self):
        if self.actors_enabled:
            self.marionette.actions.release()
        self.close_all_windows()

        super(TestPointerActions, self).tearDown()

    def test_click_action(self):
        box = self.marionette.find_element(By.ID, "testBox")
        box.get_property("localName")
        if self.actors_enabled:
            self.assertFalse(
                self.marionette.execute_script(
                    "return document.getElementById('testBox').checked"
                )
            )
            self.mouse_chain.click(element=box).perform()
            self.assertTrue(
                self.marionette.execute_script(
                    "return document.getElementById('testBox').checked"
                )
            )
        else:
            with self.assertRaises(errors.UnsupportedOperationException):
                self.mouse_chain.click(element=box).perform()

    def test_key_action(self):
        self.marionette.find_element(By.ID, "textInput").click()
        if self.actors_enabled:
            self.key_chain.send_keys("x").perform()
            self.assertEqual(
                self.marionette.execute_script(
                    "return document.getElementById('textInput').value"
                ),
                "testx",
            )
        else:
            with self.assertRaises(errors.UnsupportedOperationException):
                self.key_chain.send_keys("x").perform()
