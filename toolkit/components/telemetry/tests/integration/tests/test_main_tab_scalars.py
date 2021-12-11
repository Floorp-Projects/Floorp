# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import mozunit

from telemetry_harness.ping_filters import MAIN_SHUTDOWN_PING


def test_main_tab_scalars(browser, helpers):
    with browser.marionette.using_context(browser.marionette.CONTEXT_CHROME):
        start_tab = browser.marionette.current_window_handle
        tab2 = browser.open_tab(focus=True)
        browser.marionette.switch_to_window(tab2)
        tab3 = browser.open_tab(focus=True)
        browser.marionette.switch_to_window(tab3)
        browser.marionette.close()
        browser.marionette.switch_to_window(tab2)
        browser.marionette.close()
        browser.marionette.switch_to_window(start_tab)

    ping = helpers.wait_for_ping(browser.restart, MAIN_SHUTDOWN_PING)

    assert "main" == ping["type"]
    assert browser.get_client_id() == ping["clientId"]

    scalars = ping["payload"]["processes"]["parent"]["scalars"]

    assert 3 == scalars["browser.engagement.max_concurrent_tab_count"]
    assert 2 == scalars["browser.engagement.tab_open_event_count"]
    assert 1 == scalars["browser.engagement.max_concurrent_window_count"]


if __name__ == "__main__":
    mozunit.main()
