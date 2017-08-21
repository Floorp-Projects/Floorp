# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette_driver.errors import InvalidArgumentException

from marionette_harness import MarionetteTestCase

class TestWindowMinimize(MarionetteTestCase):

    def setUp(self):
        MarionetteTestCase.setUp(self)

        self.original_size = self.marionette.window_size

    def assert_window_minimized(self, resp):
        self.assertEqual("minimized", resp["state"])

    def assert_window_restored(self, actual):
        self.assertEqual("normal", actual["state"])
        self.assertEqual(self.original_size["width"], actual["width"])
        self.assertEqual(self.original_size["height"], actual["height"])

    def test_minimize_twice_restores(self):
        resp = self.marionette.minimize_window()
        self.assert_window_minimized(resp)

        # restore the window
        resp = self.marionette.minimize_window()
        self.assert_window_restored(resp)

    def test_minimize_stress(self):
        for i in range(1, 25):
            expect_minimized = bool(i % 2)

            resp = self.marionette.minimize_window()
            if expect_minimized:
                self.assert_window_minimized(resp)
            else:
                self.assert_window_restored(resp)
