import pytest

URL = "https://atracker.pro/"
LOGIN_CSS = "a[onclick='openWebVersionWindow()']"


def get_login_popup_url(client):
    client.execute_script("window.open = url => window.openUrl = url;")
    client.soft_click(client.await_css(LOGIN_CSS))
    return client.execute_script("return window.openUrl;")


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL)
    assert get_login_popup_url(client).find("index.html") > 0


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL)
    assert get_login_popup_url(client).find("BrowserCheck") > 0
