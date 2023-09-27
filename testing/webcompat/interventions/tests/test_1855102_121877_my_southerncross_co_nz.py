import pytest

URL = "https://my.southerncross.co.nz/browsernotsupported"
SUPPORTED_CSS = "svg.animated-success"
UNSUPPORTED_CSS = "svg.animated-failure"


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL)
    assert client.await_css(SUPPORTED_CSS)
    assert not client.find_css(UNSUPPORTED_CSS)


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL)
    assert client.await_css(SUPPORTED_CSS)
    assert client.find_css(UNSUPPORTED_CSS)
