import pytest

URL = "https://apply.flatsatshadowglen.com/manor/the-flats-at-shadowglen/"
UNSUPPORTED_CSS = "[aria-label='Your browser is not supported']"
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
