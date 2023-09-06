# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette_driver.errors import UnsupportedOperationException
from marionette_harness import MarionetteTestCase, skip


class TestReftest(MarionetteTestCase):
    def setUp(self):
        super(TestReftest, self).setUp()

        self.original_window = self.marionette.current_window_handle

        self.marionette.set_pref("marionette.log.truncate", False)
        self.marionette.set_pref("dom.send_after_paint_to_content", True)
        self.marionette.set_pref("widget.gtk.overlay-scrollbars.enabled", False)

    def tearDown(self):
        try:
            # make sure we've teared down any reftest context
            self.marionette._send_message("reftest:teardown", {})
        except UnsupportedOperationException:
            # this will throw if we aren't currently in a reftest context
            pass

        self.marionette.switch_to_window(self.original_window)

        self.marionette.clear_pref("dom.send_after_paint_to_content")
        self.marionette.clear_pref("marionette.log.truncate")
        self.marionette.clear_pref("widget.gtk.overlay-scrollbars.enabled")

        super(TestReftest, self).tearDown()

    @skip("Bug 1648444 - Unexpected page unload when refreshing about:blank")
    def test_basic(self):
        self.marionette._send_message("reftest:setup", {"screenshot": "unexpected"})
        rv = self.marionette._send_message(
            "reftest:run",
            {
                "test": "about:blank",
                "references": [["about:blank", [], "=="]],
                "expected": "PASS",
                "timeout": 10 * 1000,
            },
        )
        self.marionette._send_message("reftest:teardown", {})
        expected = {
            "value": {
                "extra": {},
                "message": "Testing about:blank == about:blank\n",
                "stack": None,
                "status": "PASS",
            }
        }
        self.assertEqual(expected, rv)

    def test_url_comparison(self):
        test_page = self.fixtures.where_is("test.html")
        test_page_2 = self.fixtures.where_is("foo/../test.html")

        self.marionette._send_message("reftest:setup", {"screenshot": "unexpected"})
        rv = self.marionette._send_message(
            "reftest:run",
            {
                "test": test_page,
                "references": [[test_page_2, [], "=="]],
                "expected": "PASS",
                "timeout": 10 * 1000,
            },
        )
        self.marionette._send_message("reftest:teardown", {})
        self.assertEqual("PASS", rv["value"]["status"])

    def test_cache_multiple_sizes(self):
        teal = self.fixtures.where_is("reftest/teal-700x700.html")
        mostly_teal = self.fixtures.where_is("reftest/mostly-teal-700x700.html")

        self.marionette._send_message("reftest:setup", {"screenshot": "unexpected"})
        rv = self.marionette._send_message(
            "reftest:run",
            {
                "test": teal,
                "references": [[mostly_teal, [], "=="]],
                "expected": "PASS",
                "timeout": 10 * 1000,
                "width": 600,
                "height": 600,
            },
        )
        self.assertEqual("PASS", rv["value"]["status"])

        rv = self.marionette._send_message(
            "reftest:run",
            {
                "test": teal,
                "references": [[mostly_teal, [], "=="]],
                "expected": "PASS",
                "timeout": 10 * 1000,
                "width": 700,
                "height": 700,
            },
        )
        self.assertEqual("FAIL", rv["value"]["status"])
        self.marionette._send_message("reftest:teardown", {})
