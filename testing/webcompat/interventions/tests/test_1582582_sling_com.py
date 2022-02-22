import pytest
from helpers import Css, assert_not_element, await_element

URL = "https://watch.sling.com/"
INCOMPATIBLE_CSS = Css("unsupported-platform")
LOADER_CSS = Css(".loader-container")


@pytest.mark.with_interventions
def test_enabled(session):
    session.get(URL)
    assert await_element(session, LOADER_CSS, timeout=20)
    assert_not_element(session, INCOMPATIBLE_CSS)


@pytest.mark.without_interventions
def test_disabled(session):
    session.get(URL)
    assert await_element(session, INCOMPATIBLE_CSS, timeout=20)
