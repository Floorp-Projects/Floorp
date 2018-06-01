# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

from marionette_driver.errors import InvalidArgumentException

from marionette_harness import MarionetteTestCase

class TestWindowMinimize(MarionetteTestCase):

    def setUp(self):
        MarionetteTestCase.setUp(self)

        self.original_size = self.marionette.window_rect

    def is_minimized(self):
        return self.marionette.execute_script("return document.hidden", sandbox=None)

    def assert_window_minimized(self, resp):
        self.assertEqual("minimized", resp["state"])

    def assert_window_restored(self, actual):
        self.assertEqual("normal", actual["state"])

    def test_minimize_twice_is_idempotent(self):
        self.assert_window_minimized(self.marionette.minimize_window())
        self.assert_window_minimized(self.marionette.minimize_window())

    def test_minimize_stress(self):
        for i in range(1, 25):
            if self.is_minimized:
                resp = self.marionette.set_window_rect(width=800, height=600)
                self.assert_window_restored(resp)
            else:
                resp = self.marionette.minimize_window()
                self.assert_window_minimized(resp)
