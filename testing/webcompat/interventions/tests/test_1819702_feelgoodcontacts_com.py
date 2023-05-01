import pytest

URL = "https://feelgoodcontacts.com/"
MOBILE_CSS = "#aHomeMob"
DESKTOP_CSS = "#mmenu"


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL)
    assert client.await_css(MOBILE_CSS)


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL)
    assert client.await_css(DESKTOP_CSS)
