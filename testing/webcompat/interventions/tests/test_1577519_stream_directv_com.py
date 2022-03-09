import pytest
from helpers import Css, Xpath, await_first_element_of

URL = "https://stream.directv.com/"
LOGIN_CSS = Css("#userID")
UNSUPPORTED_CSS = Css(".title-new-browse-ff")
DENIED_XPATH = Xpath("//h1[text()='Access Denied']")


def check_site(session, should_pass):
    session.get(URL)

    [denied, login, unsupported] = await_first_element_of(
        session, [DENIED_XPATH, LOGIN_CSS, UNSUPPORTED_CSS], is_displayed=True
    )

    if denied:
        pytest.skip("Region-locked, cannot test. Try using a VPN set to USA.")
        return

    assert (should_pass and login) or (not should_pass and unsupported)


@pytest.mark.with_interventions
def test_enabled(session):
    check_site(session, should_pass=True)


@pytest.mark.without_interventions
def test_disabled(session):
    check_site(session, should_pass=False)
