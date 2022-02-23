import pytest
import time
from helpers import Css, Text, assert_not_element, await_element, find_element

URL = "https://covid.cdc.gov/covid-data-tracker/#pandemic-vulnerability-index"

TITLE_CSS = Css("#mainContent_Title")
UNSUPPORTED_TEXT = Text("not support Firefox")


@pytest.mark.with_interventions
def test_enabled(session):
    session.get(URL)
    time.sleep(4)
    assert find_element(session, TITLE_CSS)
    assert_not_element(session, UNSUPPORTED_TEXT)


@pytest.mark.without_interventions
def test_disabled(session):
    session.get(URL)
    assert await_element(session, UNSUPPORTED_TEXT)
