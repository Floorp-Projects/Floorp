import pytest
from support.helpers import read_user_preferences
from tests.support.sync import Poll


@pytest.mark.parametrize(
    "value",
    [
        {"pref_value": 1, "use_cdp": False, "use_bidi": True},
        {"pref_value": 2, "use_cdp": True, "use_bidi": False},
        {"pref_value": 3, "use_cdp": True, "use_bidi": True},
    ],
    ids=["bidi only", "cdp only", "bidi and cdp"],
)
def test_remote_agent_recommended_preferences_applied(browser, value):
    # Marionette cannot be enabled for this test because it will also set the
    # recommended preferences. Therefore only enable Remote Agent protocols.
    current_browser = browser(
        extra_prefs={
            "remote.active-protocols": value["pref_value"],
        },
        use_cdp=value["use_cdp"],
        use_bidi=value["use_bidi"],
    )

    def pref_is_set(_):
        preferences = read_user_preferences(current_browser.profile.profile, "prefs.js")
        return preferences.get("remote.prefs.recommended.applied", False)

    # Without Marionette enabled preferences cannot be retrieved via script evaluation yet.
    wait = Poll(
        None,
        timeout=5,
        ignored_exceptions=IOError,
        message="""Preference "remote.prefs.recommended.applied" is not true""",
    )
    wait.until(pref_is_set)
