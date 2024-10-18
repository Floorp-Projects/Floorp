import pytest

URL = "https://digits.t-mobile.com/"
BLOCKED_CSS = "#incompatible"
NOT_BLOCKED_CSS = "#compatible"


@pytest.mark.skip_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL)
    assert client.await_css(NOT_BLOCKED_CSS)
    assert not client.find_css(BLOCKED_CSS)


@pytest.mark.skip_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL)
    assert client.await_css(BLOCKED_CSS)
    assert not client.find_css(NOT_BLOCKED_CSS)
