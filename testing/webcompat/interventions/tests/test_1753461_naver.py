import pytest
from helpers import Css, await_element

URL = "https://serieson.naver.com/v2/movie/335790?isWebtoonAgreePopUp=true"

SUPPORTED_CSS = Css("#playerWrapper")
UNSUPPORTED_CSS = Css(".end_player_unavailable .download_links")


@pytest.mark.with_interventions
def test_enabled(session):
    session.get(URL)
    assert await_element(session, SUPPORTED_CSS)


@pytest.mark.without_interventions
def test_disabled(session):
    session.get(URL)
    assert await_element(session, UNSUPPORTED_CSS)
