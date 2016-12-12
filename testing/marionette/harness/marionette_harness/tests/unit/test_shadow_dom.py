# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette_driver.by import By
from marionette_driver.errors import (
    NoSuchElementException,
    StaleElementException
)

from marionette_harness import MarionetteTestCase


class TestShadowDom(MarionetteTestCase):

    def setUp(self):
        MarionetteTestCase.setUp(self)
        self.marionette.enforce_gecko_prefs({"dom.webcomponents.enabled": True})
        self.marionette.navigate(self.marionette.absolute_url("test_shadow_dom.html"))

        self.host = self.marionette.find_element(By.ID, "host")
        self.marionette.switch_to_shadow_root(self.host)
        self.button = self.marionette.find_element(By.ID, "button")

    def test_shadow_dom(self):
        # Button in shadow root should be actionable
        self.button.click()

    def test_shadow_dom_after_switch_away_from_shadow_root(self):
        # Button in shadow root should be actionable
        self.button.click()
        self.marionette.switch_to_shadow_root()
        # After switching back to top content, button should be stale
        self.assertRaises(StaleElementException, self.button.click)

    def test_shadow_dom_raises_stale_element_exception_when_button_remove(self):
        self.marionette.execute_script(
            'document.getElementById("host").shadowRoot.getElementById("button").remove();')
        # After removing button from shadow DOM, button should be stale
        self.assertRaises(StaleElementException, self.button.click)

    def test_shadow_dom_raises_stale_element_exception_when_host_removed(self):
        self.marionette.execute_script('document.getElementById("host").remove();')
        # After removing shadow DOM host element, button should be stale
        self.assertRaises(StaleElementException, self.button.click)

    def test_non_existent_shadow_dom(self):
        # Jump back to top level content
        self.marionette.switch_to_shadow_root()
        # When no ShadowRoot is found, switch_to_shadow_root throws NoSuchElementException
        self.assertRaises(NoSuchElementException, self.marionette.switch_to_shadow_root,
                          self.marionette.find_element(By.ID, "empty-host"))

    def test_inner_shadow_dom(self):
        # Button in shadow root should be actionable
        self.button.click()
        self.inner_host = self.marionette.find_element(By.ID, "inner-host")
        self.marionette.switch_to_shadow_root(self.inner_host)
        self.inner_button = self.marionette.find_element(By.ID, "inner-button")
        # Nested nutton in nested shadow root should be actionable
        self.inner_button.click()
        self.marionette.switch_to_shadow_root()
        # After jumping back to parent shadow root, button should again be actionable but inner
        # button should now be stale
        self.button.click()
        self.assertRaises(StaleElementException, self.inner_button.click)
        self.marionette.switch_to_shadow_root()
        # After switching back to top content, both buttons should now be stale
        self.assertRaises(StaleElementException, self.button.click)
        self.assertRaises(StaleElementException, self.inner_button.click)
