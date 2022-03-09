import time
import pytest
from helpers import Css, await_element, find_element

URL = "https://www.bankofamerica.com/"


@pytest.mark.with_interventions
def test_enabled(session):
    session.get(URL)
    time.sleep(3)
    assert find_element(session, Css("#browserUpgradeNoticeBar"), default=None) is None


@pytest.mark.without_interventions
def test_disabled(session):
    session.get(URL)
    warning = await_element(session, Css("#browserUpgradeNoticeBar"), timeout=3)
    assert warning.is_displayed()
