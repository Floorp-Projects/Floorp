import pytest
from helpers import Css, Text, await_first_element_of

URL = "https://www.directv.com.co/"
INCOMPATIBLE_CSS = Css(".browser-compatible.compatible.incompatible")
DENIED_TEXT = Text("not available in your region")


def check_unsupported_visibility(session, should_show):
    session.get(URL)

    [denied, incompatible] = await_first_element_of(
        session, [DENIED_TEXT, INCOMPATIBLE_CSS]
    )

    if denied:
        pytest.skip("Region-locked, cannot test. Try using a VPN set to USA.")
        return

    shown = incompatible.is_displayed()
    assert (should_show and shown) or (not should_show and not shown)


@pytest.mark.with_interventions
def test_enabled(session):
    check_unsupported_visibility(session, should_show=False)


@pytest.mark.without_interventions
def test_disabled(session):
    check_unsupported_visibility(session, should_show=True)
