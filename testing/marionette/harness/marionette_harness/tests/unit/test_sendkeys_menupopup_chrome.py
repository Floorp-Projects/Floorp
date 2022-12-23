# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from six.moves.urllib.parse import quote

from marionette_driver import By, errors, Wait
from marionette_driver.keys import Keys

from marionette_harness import (
    MarionetteTestCase,
    WindowManagerMixin,
)


class TestSendkeysMenupopup(WindowManagerMixin, MarionetteTestCase):
    def setUp(self):
        super(TestSendkeysMenupopup, self).setUp()

        self.marionette.set_context("chrome")
        new_window = self.open_chrome_window(
            "chrome://remote/content/marionette/test_menupopup.xhtml"
        )
        self.marionette.switch_to_window(new_window)

        self.click_el = self.marionette.find_element(By.ID, "options-button")
        self.disabled_menuitem_el = self.marionette.find_element(
            By.ID, "option-disabled"
        )
        self.hidden_menuitem_el = self.marionette.find_element(By.ID, "option-hidden")
        self.menuitem_el = self.marionette.find_element(By.ID, "option-enabled")
        self.menupopup_el = self.marionette.find_element(By.ID, "options-menupopup")
        self.testwindow_el = self.marionette.find_element(By.ID, "test-window")

    def context_menu_state(self):
        return self.menupopup_el.get_property("state")

    def open_context_menu(self):
        def attempt_open_context_menu():
            self.assertEqual(self.context_menu_state(), "closed")
            self.click_el.click()
            Wait(self.marionette).until(
                lambda _: self.context_menu_state() == "open",
                message="Context menu did not open",
            )

        try:
            attempt_open_context_menu()
        except errors.TimeoutException:
            # If the first attempt timed out, try a second time.
            # On Linux, the test will intermittently fail if we click too
            # early on the button. Retrying fixes the issue. See Bug 1686769.
            attempt_open_context_menu()

    def wait_for_context_menu_closed(self):
        Wait(self.marionette).until(
            lambda _: self.context_menu_state() == "closed",
            message="Context menu did not close",
        )

    def tearDown(self):
        try:
            self.close_all_windows()
        finally:
            super(TestSendkeysMenupopup, self).tearDown()

    def test_sendkeys_menuitem(self):
        # Try closing the context menu by sending ESCAPE to a visible context menu item.
        self.open_context_menu()

        self.menuitem_el.send_keys(Keys.ESCAPE)
        self.wait_for_context_menu_closed()

    def test_sendkeys_menupopup(self):
        # Try closing the context menu by sending ESCAPE to the context menu.
        self.open_context_menu()

        self.menupopup_el.send_keys(Keys.ESCAPE)
        self.wait_for_context_menu_closed()

    def test_sendkeys_window(self):
        # Try closing the context menu by sending ESCAPE to the main window.
        self.open_context_menu()

        self.testwindow_el.send_keys(Keys.ESCAPE)
        self.wait_for_context_menu_closed()

    def test_sendkeys_closed_menu(self):
        # send_keys should throw for the menupopup if the contextmenu is closed.
        with self.assertRaises(errors.ElementNotInteractableException):
            self.menupopup_el.send_keys(Keys.ESCAPE)

        # send_keys should throw for the menuitem if the contextmenu is closed.
        with self.assertRaises(errors.ElementNotInteractableException):
            self.menuitem_el.send_keys(Keys.ESCAPE)

    def test_sendkeys_hidden_disabled_menuitem(self):
        self.open_context_menu()

        # send_keys should throw for a disabled menuitem in an opened contextmenu.
        with self.assertRaises(errors.ElementNotInteractableException):
            self.disabled_menuitem_el.send_keys(Keys.ESCAPE)

        # send_keys should throw for a hidden menuitem in an opened contextmenu.
        with self.assertRaises(errors.ElementNotInteractableException):
            self.hidden_menuitem_el.send_keys(Keys.ESCAPE)
