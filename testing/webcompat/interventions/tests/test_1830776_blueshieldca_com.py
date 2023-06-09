import pytest

URL = "https://www.blueshieldca.com/provider"
UNSUPPORTED_CSS = "#browserErrorModal"


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL)
    assert client.find_css(UNSUPPORTED_CSS, is_displayed=False)


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL)
    assert client.find_css(UNSUPPORTED_CSS, is_displayed=True)
