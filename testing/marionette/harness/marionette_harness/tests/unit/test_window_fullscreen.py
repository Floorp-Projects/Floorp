# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

from marionette_harness import MarionetteTestCase


class TestWindowFullscreen(MarionetteTestCase):

    def setUp(self):
        MarionetteTestCase.setUp(self)
        self.max = self.marionette.execute_script("""
            return {
              width: window.screen.availWidth,
              height: window.screen.availHeight,
            }""", sandbox=None)

        # ensure window is not fullscreen
        actual = self.marionette.set_window_rect(
            width=self.max["width"] - 100, height=self.max["height"] - 100)
        self.assertNotEqual(actual["width"], self.max["width"])
        self.assertNotEqual(actual["height"], self.max["height"])

        self.original_size = actual

    def tearDown(self):
        fullscreen = self.marionette.execute_script("""
            return window.fullScreen;""", sandbox=None)
        if fullscreen:
            self.marionette.fullscreen()

    def assert_window_fullscreen(self, actual):
        self.assertTrue(self.marionette.execute_script(
            "return window.fullScreen", sandbox=None))

    def assert_window_restored(self, actual):
        self.assertEqual(self.original_size["width"], actual["width"])
        self.assertEqual(self.original_size["height"], actual["height"])
        self.assertFalse(self.marionette.execute_script(
            "return window.fullScreen", sandbox=None))

    def assert_window_rect(self, rect):
        self.assertIn("width", rect)
        self.assertIn("height", rect)
        self.assertIn("x", rect)
        self.assertIn("y", rect)
        self.assertIsInstance(rect["width"], int)
        self.assertIsInstance(rect["height"], int)
        self.assertIsInstance(rect["x"], int)
        self.assertIsInstance(rect["y"], int)

    def test_fullscreen(self):
        rect = self.marionette.fullscreen()
        self.assert_window_rect(rect)
        size = self.marionette.window_size
        self.assertEqual(size, rect)
        self.assert_window_fullscreen(size)

    def test_fullscreen_twice_is_idempotent(self):
        self.assert_window_fullscreen(self.marionette.fullscreen())
        self.assert_window_fullscreen(self.marionette.fullscreen())
