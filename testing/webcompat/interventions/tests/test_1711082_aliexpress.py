import pytest
from helpers import Css, await_element, find_element


URL = "https://m.aliexpress.com/?tracelog=wwwhome2mobilesitehome"
SELECTOR = Css("#header input[placeholder]")
DISABLED_SELECTOR = Css(f"{SELECTOR.value}:disabled")


@pytest.mark.with_interventions
def test_enabled(session):
    session.get(URL)
    find_element(session, SELECTOR)
    assert find_element(session, DISABLED_SELECTOR, default=None) is None


@pytest.mark.without_interventions
def test_disabled(session):
    session.get(URL)
    await_element(session, SELECTOR)
    find_element(session, DISABLED_SELECTOR)
