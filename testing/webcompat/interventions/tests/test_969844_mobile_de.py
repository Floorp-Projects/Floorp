import pytest

# The site serves a different mobile page to Firefox than Chrome

URL = "https://www.mobile.de/"
MOBILE_CSS = "header [data-testid='header-burger-menu']"
DESKTOP_CSS = "header [data-testid='header-mobile-small-screen']"


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL)
    assert client.await_css(MOBILE_CSS)
    assert not client.find_css(DESKTOP_CSS)


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL)
    assert client.await_css(DESKTOP_CSS)
    assert not client.find_css(MOBILE_CSS)
