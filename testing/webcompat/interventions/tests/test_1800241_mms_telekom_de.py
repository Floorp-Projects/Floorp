import pytest
from helpers import Css, assert_not_element, await_element

URL = "https://www.mms.telekom.de/OTP/"
LOGIN_CSS = Css(".LoginbackgroundDiv")


@pytest.mark.with_interventions
def test_enabled(session):
    session.get(URL)
    login_element = await_element(session, LOGIN_CSS)
    assert login_element.is_displayed()


@pytest.mark.without_interventions
def test_disabled(session):
    session.get(URL)
    assert_not_element(session, LOGIN_CSS)
