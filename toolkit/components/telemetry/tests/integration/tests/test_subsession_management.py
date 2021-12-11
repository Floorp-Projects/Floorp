# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import mozunit

from telemetry_harness.ping_filters import (
    MAIN_ENVIRONMENT_CHANGE_PING,
    MAIN_SHUTDOWN_PING,
)


def test_subsession_management(browser, helpers):
    """Test for Firefox Telemetry subsession management."""

    # Session S1, subsession 1
    # Actions:
    # 1. Open browser
    # 2. Open a new tab
    # 3. Restart browser in new session

    with browser.new_tab():
        # If Firefox Telemetry is working correctly, this will
        # be sufficient to record a tab open event.
        pass

    ping1 = helpers.wait_for_ping(browser.restart, MAIN_SHUTDOWN_PING)

    # Session S2, subsession 1
    # Outcome 1:
    # Received a main ping P1 for previous session
    # - Ping base contents:
    #     - clientId should be a valid UUID
    #     - reason should be "shutdown"
    #     - sessionId should be set
    #     - subsessionId should be set
    #     - previousSessionId should not be set
    #     - previousSubsessionId should not be set
    #     - subSessionCounter should be 1
    #     - profileSubSessionCounter should be 1
    # - Other ping contents:
    #     - tab_open_event_count in scalars

    client_id = ping1["clientId"]
    helpers.assert_is_valid_uuid(client_id)

    ping1_info = ping1["payload"]["info"]
    assert ping1_info["reason"] == "shutdown"

    s1_session_id = ping1_info["sessionId"]
    assert s1_session_id != ""

    s1_s1_subsession_id = ping1_info["subsessionId"]
    assert s1_s1_subsession_id != ""
    assert ping1_info["previousSessionId"] is None
    assert ping1_info["previousSubsessionId"] is None
    assert ping1_info["subsessionCounter"] == 1
    assert ping1_info["profileSubsessionCounter"] == 1

    scalars1 = ping1["payload"]["processes"]["parent"]["scalars"]
    assert "browser.engagement.window_open_event_count" not in scalars1
    assert scalars1["browser.engagement.tab_open_event_count"] == 1

    # Actions:
    # 1. Install addon

    ping2 = helpers.wait_for_ping(browser.install_addon, MAIN_ENVIRONMENT_CHANGE_PING)

    [addon_id] = browser.addon_ids  # Store the addon ID for verifying ping3 later

    # Session S2, subsession 2
    # Outcome 2:
    # Received a main ping P2 for previous subsession
    # - Ping base contents:
    #     - clientId should be set to the same value
    #     - sessionId should be set to a new value
    #     - subsessionId should be set to a new value
    #     - previousSessionId should be set to P1s sessionId value
    #     - previousSubsessionId should be set to P1s subsessionId value
    #     - subSessionCounter should be 1
    #     - profileSubSessionCounter should be 2
    #     - reason should be "environment-change"
    # - Other ping contents:
    #     - tab_open_event_count not in scalars

    assert ping2["clientId"] == client_id

    ping2_info = ping2["payload"]["info"]
    assert ping2_info["reason"] == "environment-change"

    s2_session_id = ping2_info["sessionId"]
    assert s2_session_id != s1_session_id

    s2_s1_subsession_id = ping2_info["subsessionId"]
    assert s2_s1_subsession_id != s1_s1_subsession_id
    assert ping2_info["previousSessionId"] == s1_session_id
    assert ping2_info["previousSubsessionId"] == s1_s1_subsession_id
    assert ping2_info["subsessionCounter"] == 1
    assert ping2_info["profileSubsessionCounter"] == 2

    scalars2 = ping2["payload"]["processes"]["parent"]["scalars"]
    assert "browser.engagement.window_open_event_count" not in scalars2
    assert "browser.engagement.tab_open_event_count" not in scalars2

    # Actions
    # 1. Restart browser in new session

    ping3 = helpers.wait_for_ping(browser.restart, MAIN_SHUTDOWN_PING)

    # Session S3, subsession 1
    # Outcome 3:
    # Received a main ping P3 for session 2, subsession 2
    # - Ping base contents:
    #     - clientId should be set to the same value
    #     - sessionId should be set to P2s sessionId value
    #     - subsessionId should be set to a new value
    #     - previousSessionId should be set to P1s sessionId value
    #     - previousSubsessionId should be set to P2s subsessionId value
    #     - subSessionCounter should be 2
    #     - profileSubSessionCounter should be 3
    #     - reason should be "shutdown"
    # - Other ping contents:
    #     - addon ID in activeAddons in environment

    assert ping3["clientId"] == client_id

    ping3_info = ping3["payload"]["info"]
    assert ping3_info["reason"] == "shutdown"

    assert ping3_info["sessionId"] == s2_session_id

    s2_s2_subsession_id = ping3_info["subsessionId"]
    assert s2_s2_subsession_id != s1_s1_subsession_id
    assert s2_s2_subsession_id != s2_s1_subsession_id
    assert ping3_info["previousSessionId"] == s1_session_id
    assert ping3_info["previousSubsessionId"] == s2_s1_subsession_id
    assert ping3_info["subsessionCounter"] == 2
    assert ping3_info["profileSubsessionCounter"] == 3

    scalars3 = ping3["payload"]["processes"]["parent"]["scalars"]
    assert "browser.engagement.window_open_event_count" not in scalars3
    assert "browser.engagement.tab_open_event_count" not in scalars3

    active_addons = ping3["environment"]["addons"]["activeAddons"]
    assert addon_id in active_addons


if __name__ == "__main__":
    mozunit.main()
