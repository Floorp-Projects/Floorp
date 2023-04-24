import pytest

URL = "https://www.autotrader.ca/"
MOBILE_CSS = "#openInApp"


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL)
    assert client.find_css(MOBILE_CSS)


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL)
    assert not client.find_css(MOBILE_CSS)
