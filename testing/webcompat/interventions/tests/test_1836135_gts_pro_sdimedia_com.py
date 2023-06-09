import pytest

URL = "https://gts-pro.sdimedia.com/"
UNSUPPORTED_CSS = "#invalid-browser"
LOGIN_CSS = "button[data-qa='open-login-page']"


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL)
    assert client.await_css(LOGIN_CSS, is_displayed=True)


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL)
    assert client.await_css(UNSUPPORTED_CSS, is_displayed=True)
