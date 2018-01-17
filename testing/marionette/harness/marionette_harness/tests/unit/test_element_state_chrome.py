# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

from marionette_driver.by import By

from marionette_harness import MarionetteTestCase, skip, WindowManagerMixin


class TestElementState(WindowManagerMixin, MarionetteTestCase):

    def setUp(self):
        super(TestElementState, self).setUp()

        self.marionette.set_context("chrome")

        def open_window_with_js():
            self.marionette.execute_script("""
              window.open('chrome://marionette/content/test.xul',
                          'foo', 'chrome,centerscreen');
            """)

        self.win = self.open_window(open_window_with_js)
        self.marionette.switch_to_window(self.win)

    def tearDown(self):
        self.close_all_windows()

        super(TestElementState, self).tearDown()

    @skip("Switched off in bug 896043, and to be turned on in bug 896046")
    def test_is_displayed(self):
        l = self.marionette.find_element(By.ID, "textInput")
        self.assertTrue(l.is_displayed())
        self.marionette.execute_script("arguments[0].hidden = true;", [l])
        self.assertFalse(l.is_displayed())
        self.marionette.execute_script("arguments[0].hidden = false;", [l])

    def test_enabled(self):
        l = self.marionette.find_element(By.ID, "textInput")
        self.assertTrue(l.is_enabled())
        self.marionette.execute_script("arguments[0].disabled = true;", [l])
        self.assertFalse(l.is_enabled())
        self.marionette.execute_script("arguments[0].disabled = false;", [l])

    def test_can_get_element_rect(self):
        l = self.marionette.find_element(By.ID, "textInput")
        rect = l.rect
        self.assertTrue(rect['x'] > 0)
        self.assertTrue(rect['y'] > 0)

    def test_get_attribute(self):
        el = self.marionette.execute_script("return window.document.getElementById('textInput');")
        self.assertEqual(el.get_attribute("id"), "textInput")

    def test_get_property(self):
        el = self.marionette.execute_script("return window.document.getElementById('textInput');")
        self.assertEqual(el.get_property("id"), "textInput")
