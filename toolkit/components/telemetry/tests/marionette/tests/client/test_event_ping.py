# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import textwrap

from telemetry_harness.testcase import TelemetryTestCase
from telemetry_harness.ping_filters import EVENT_PING


class TestEventPing(TelemetryTestCase):
    """Tests for "event" ping."""

    def enable_search_events(self):
        """
        Event Telemetry categories are disabled by default.
        Search events are in the "navigation" category and are not enabled by
        default in builds of Firefox, so we enable them here.
        """

        script = """\
        let {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");
        Services.telemetry.setEventRecordingEnabled("navigation", true);
        """

        with self.marionette.using_context(self.marionette.CONTEXT_CHROME):
            self.marionette.execute_script(textwrap.dedent(script))

    def wait_for_search_service_init(self):
        script = """\
        let [resolve] = arguments;
        let searchService = Components.classes["@mozilla.org/browser/search-service;1"]
            .getService(Components.interfaces.nsISearchService);
        searchService.init().then(resolve);
        """

        with self.marionette.using_context(self.marionette.CONTEXT_CHROME):
            self.marionette.execute_async_script(textwrap.dedent(script))

    def test_event_ping(self):
        """
        Barebones test for "event" ping:
        Search, close Firefox, check "event" ping for search events.
        """

        self.enable_search_events()
        self.wait_for_search_service_init()

        self.search("mozilla firefox")

        payload = self.wait_for_ping(self.restart_browser, EVENT_PING)["payload"]

        self.assertEqual(payload["reason"], "shutdown")
        self.assertEqual(payload["lostEventsCount"], 0)

        self.assertIn("events", payload)
        self.assertIn("parent", payload["events"])
        found_it = False

        for event in payload["events"]["parent"]:
            # The event may optionally contain additonal fields
            [timestamp, category, method, obj] = event[:4]

            self.assertTrue(timestamp > 0)
            if category == "navigation" and method == "search" and obj == "urlbar":
                found_it = True

        self.assertTrue(found_it)
