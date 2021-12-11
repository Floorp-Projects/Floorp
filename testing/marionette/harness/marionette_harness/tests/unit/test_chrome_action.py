# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

from marionette_driver import By
from marionette_driver.keys import Keys

from marionette_harness import MarionetteTestCase, WindowManagerMixin


class TestPointerActions(WindowManagerMixin, MarionetteTestCase):
    def setUp(self):
        super(TestPointerActions, self).setUp()

        self.mouse_chain = self.marionette.actions.sequence(
            "pointer", "pointer_id", {"pointerType": "mouse"}
        )
        self.key_chain = self.marionette.actions.sequence("key", "keyboard_id")

        if self.marionette.session_capabilities["platformName"] == "mac":
            self.mod_key = Keys.META
        else:
            self.mod_key = Keys.CONTROL

        self.marionette.set_context("chrome")

        self.win = self.open_chrome_window(
            "chrome://remote/content/marionette/test.xhtml"
        )
        self.marionette.switch_to_window(self.win)

    def tearDown(self):
        self.marionette.actions.release()
        self.close_all_windows()

        super(TestPointerActions, self).tearDown()

    def test_click_action(self):
        box = self.marionette.find_element(By.ID, "testBox")
        box.get_property("localName")
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

    def test_key_action(self):
        self.marionette.find_element(By.ID, "textInput").click()
        self.key_chain.send_keys("x").perform()
        self.assertEqual(
            self.marionette.execute_script(
                "return document.getElementById('textInput').value"
            ),
            "testx",
        )
