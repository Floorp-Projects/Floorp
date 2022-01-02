# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import mozunit

from telemetry_harness.ping_filters import (
    MAIN_ENVIRONMENT_CHANGE_PING,
    MAIN_SHUTDOWN_PING,
)


def test_search_counts(browser, helpers):
    """Test for SEARCH_COUNTS across sessions."""

    # Session S1, subsession 1:
    # - Open browser
    # - Open new tab
    # - Perform search (awesome bar or search bar)
    # - Restart browser in new session
    search_engine = browser.get_default_search_engine()
    browser.search_in_new_tab("mozilla firefox")
    ping1 = helpers.wait_for_ping(browser.restart, MAIN_SHUTDOWN_PING)

    # Session S2, subsession 1:
    # - Outcome 1
    # - Received a main ping P1 for previous session
    # - Ping base contents:
    #     - clientId should be set
    #     - sessionId should be set
    #     - subsessionId should be set
    #     - previousSessionId should not be set
    #     - previousSubsessionId should not be set
    #     - subSessionCounter should be 1
    #     - profileSubSessionCounter should be 1
    #     - reason should be "shutdown"
    # - Other ping contents:
    #     - SEARCH_COUNTS values should match performed search action

    client_id = ping1["clientId"]
    helpers.assert_is_valid_uuid(client_id)

    ping1_info = ping1["payload"]["info"]
    assert "shutdown" == ping1_info["reason"]

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

    keyed_histograms1 = ping1["payload"]["keyedHistograms"]
    search_counts1 = keyed_histograms1["SEARCH_COUNTS"][
        "{}.urlbar".format(search_engine)
    ]

    assert search_counts1 == {
        u"range": [1, 2],
        u"bucket_count": 3,
        u"histogram_type": 4,
        u"values": {u"1": 0, u"0": 1},
        u"sum": 1,
    }

    # - Install addon
    # Session S2, subsession 2:
    # - Outcome 2
    # - Received a main ping P2 for previous subsession
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
    #     - SEARCH_COUNTS values should not be in P2
    # - Verify that there should be no listing for tab scalar as we started a new
    # session

    ping2 = helpers.wait_for_ping(browser.install_addon, MAIN_ENVIRONMENT_CHANGE_PING)

    assert client_id == ping2["clientId"]

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

    keyed_histograms2 = ping2["payload"]["keyedHistograms"]
    assert "SEARCH_COUNTS" not in keyed_histograms2

    # - Perform Search
    # - Restart Browser

    browser.search("mozilla telemetry")
    browser.search("python unittest")
    browser.search("python pytest")

    ping3 = helpers.wait_for_ping(browser.restart, MAIN_SHUTDOWN_PING)

    # Session S3, subsession 1:
    # - Outcome 3
    #   - Received a main ping P3 for session 2, subsession 1
    #   - Ping base contents:
    #     - clientId should be set to the same value
    #     - sessionId should be set to P2s sessionId value
    #     - subsessionId should be set to a new value
    #     - previousSessionId should be set to P1s sessionId value
    #     - previousSubsessionId should be set to P2s subsessionId value
    #     - subSessionCounter should be 2
    #     - profileSubSessionCounter should be 3
    #     - reason should be "shutdown"
    #   - Other ping contents:
    #     - SEARCH_COUNTS values should be set per above search

    assert client_id == ping3["clientId"]

    ping3_info = ping3["payload"]["info"]
    assert ping3_info["reason"] == "shutdown"
    assert ping3_info["sessionId"] == s2_session_id

    s2_s2_subsession_id = ping3_info["subsessionId"]
    assert s2_s2_subsession_id != s1_s1_subsession_id
    assert ping3_info["previousSessionId"] == s1_session_id
    assert ping3_info["previousSubsessionId"] == s2_s1_subsession_id
    assert ping3_info["subsessionCounter"] == 2
    assert ping3_info["profileSubsessionCounter"] == 3

    scalars3 = ping3["payload"]["processes"]["parent"]["scalars"]
    assert "browser.engagement.window_open_event_count" not in scalars3

    keyed_histograms3 = ping3["payload"]["keyedHistograms"]
    search_counts3 = keyed_histograms3["SEARCH_COUNTS"][
        "{}.urlbar".format(search_engine)
    ]
    assert search_counts3 == {
        u"range": [1, 2],
        u"bucket_count": 3,
        u"histogram_type": 4,
        u"values": {u"1": 0, u"0": 3},
        u"sum": 3,
    }


if __name__ == "__main__":
    mozunit.main()
