import pytest

URL = "https://serieson.naver.com/v2/movie/335790?isWebtoonAgreePopUp=true"

SUPPORTED_CSS = "#playerWrapper"
UNSUPPORTED_CSS = ".end_player_unavailable .download_links"


@pytest.mark.skip_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL)
    assert client.await_css(SUPPORTED_CSS)


@pytest.mark.skip_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL)
    assert client.await_css(UNSUPPORTED_CSS)
