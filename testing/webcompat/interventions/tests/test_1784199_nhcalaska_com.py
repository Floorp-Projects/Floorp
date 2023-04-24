import pytest

URL = "https://www.nhcalaska.com/"
UNSUPPORTED_CSS = "[aria-label='Your browser is not supported']"


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL)
    unsupported = client.await_css(UNSUPPORTED_CSS)
    assert not client.is_displayed(unsupported)


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL)
    unsupported = client.await_css(UNSUPPORTED_CSS)
    assert client.is_displayed(unsupported)
