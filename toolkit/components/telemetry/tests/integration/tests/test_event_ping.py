# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import mozunit

from telemetry_harness.ping_filters import EVENT_PING


def test_event_ping(browser, helpers):
    """
    Barebones test for "event" ping:
    Search, close Firefox, check "event" ping for search events.
    """
    browser.enable_search_events()
    browser.wait_for_search_service_init()
    browser.search("mozilla firefox")

    payload = helpers.wait_for_ping(browser.restart, EVENT_PING)["payload"]

    assert "shutdown" == payload["reason"]
    assert 0 == payload["lostEventsCount"]
    assert "events" in payload
    assert "parent" in payload["events"]
    assert find_event(payload["events"]["parent"])


def find_event(events):
    """Return the first event that has the expected timestamp, category method and object"""

    for event in events:
        # The event may optionally contain additonal fields
        [timestamp, category, method, object_id] = event[:4]

        assert timestamp > 0

        if category != "navigation":
            continue

        if method != "search":
            continue

        if object_id != "urlbar":
            continue

        return True

    return False


if __name__ == "__main__":
    mozunit.main()
