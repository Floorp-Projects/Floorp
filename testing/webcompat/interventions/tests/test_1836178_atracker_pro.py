import pytest

URL = "https://atracker.pro/"
LOGIN_CSS = "a[onclick='openWebVersionWindow()']"


async def get_login_popup_url(client, popup_url):
    popup = await client.await_popup(popup_url)
    await client.navigate(URL, wait="load")
    client.soft_click(client.await_css(LOGIN_CSS))
    return await popup


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    assert await get_login_popup_url(client, "index.html")


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    assert await get_login_popup_url(client, "BrowserCheck")
