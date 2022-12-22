import pytest

URL = "https://www.mms.telekom.de/OTP/"
LOGIN_CSS = ".LoginbackgroundDiv"


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL)
    login_element = client.await_css(LOGIN_CSS)
    assert client.is_displayed(login_element)


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL)
    assert not client.find_css(LOGIN_CSS)
