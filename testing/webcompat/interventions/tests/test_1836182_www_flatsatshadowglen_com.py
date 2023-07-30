import pytest

URL = "https://apply.flatsatshadowglen.com/manor/the-flats-at-shadowglen/"
UNSUPPORTED_CSS = "[aria-label='Your browser is not supported']"


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL)
    assert not client.find_css(UNSUPPORTED_CSS)


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL)
    assert client.find_css(UNSUPPORTED_CSS)
