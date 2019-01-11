# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function

from marionette_driver.errors import InvalidArgumentException, UnsupportedOperationException
from marionette_harness import MarionetteTestCase


class TestReftest(MarionetteTestCase):
    def setUp(self):
        super(TestReftest, self).setUp()

        self.original_window = self.marionette.current_window_handle

        self.marionette.set_context(self.marionette.CONTEXT_CHROME)
        self.marionette.set_pref("dom.send_after_paint_to_content", True)

    def tearDown(self):
        try:
            # make sure we've teared down any reftest context
            self.marionette._send_message("reftest:teardown", {})
        except UnsupportedOperationException:
            # this will throw if we aren't currently in a reftest context
            pass

        self.marionette.switch_to_window(self.original_window)

        self.marionette.set_context(self.marionette.CONTEXT_CONTENT)
        self.marionette.clear_pref("dom.send_after_paint_to_content")

        super(TestReftest, self).tearDown()

    def test_basic(self):
        self.marionette._send_message("reftest:setup", {"screenshot": "unexpected"})
        rv = self.marionette._send_message("reftest:run",
                                           {"test": "about:blank",
                                            "references": [["about:blank", [], "=="]],
                                            "expected": "PASS",
                                            "timeout": 10 * 1000})
        self.marionette._send_message("reftest:teardown", {})
        expected = {u'value': {u'extra': {},
                               u'message': u'Testing about:blank == about:blank\n',
                               u'stack': None,
                               u'status': u'PASS'}}
        self.assertEqual(expected, rv)
