import pytest

# The site serves a different mobile page to Firefox than Chrome

URL = "https://www.mobile.de/"
COOKIES_CSS = "button.mde-consent-accept-btn"
MOBILE_MENU_CSS = "header [data-testid='mobile-navigation-menu']"


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL)
    client.soft_click(client.await_css(COOKIES_CSS))
    mobile_menu = client.await_css(MOBILE_MENU_CSS)
    assert client.is_displayed(mobile_menu)


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL)
    client.soft_click(client.await_css(COOKIES_CSS))
    mobile_menu = client.await_css(MOBILE_MENU_CSS)
    assert not client.is_displayed(mobile_menu)
