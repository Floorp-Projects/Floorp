import pytest

URL = "https://store-kr.uniqlo.com/"
REDIRECT_CSS = "[onload='javascript:redirectPage();']"
APP_CSS = "uni-root"


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL)
    assert client.await_css(APP_CSS)


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL)
    assert client.await_css(REDIRECT_CSS)
