import pytest
from helpers import Css, await_element

URL = "https://m.pji.co.kr/"
UNSUPPORTED_BANNER = Css("#chromAlarmView")


@pytest.mark.with_interventions
def test_enabled(session):
    session.get(URL)
    unsupported_banner = await_element(session, UNSUPPORTED_BANNER)
    assert unsupported_banner.is_displayed() is False


@pytest.mark.without_interventions
def test_disabled(session):
    session.get(URL)
    unsupported_banner = await_element(session, UNSUPPORTED_BANNER)
    assert unsupported_banner.is_displayed()
