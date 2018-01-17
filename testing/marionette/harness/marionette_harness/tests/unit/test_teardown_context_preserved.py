# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

from marionette_harness import MarionetteTestCase, SkipTest


class TestTearDownContext(MarionetteTestCase):
    def setUp(self):
        MarionetteTestCase.setUp(self)
        self.marionette.set_context(self.marionette.CONTEXT_CHROME)

    def tearDown(self):
        self.assertEqual(self.get_context(), self.marionette.CONTEXT_CHROME)
        MarionetteTestCase.tearDown(self)

    def get_context(self):
        return self.marionette._send_message("getContext", key="value")

    def test_skipped_teardown_ok(self):
        raise SkipTest("This should leave our teardown method in chrome context")
