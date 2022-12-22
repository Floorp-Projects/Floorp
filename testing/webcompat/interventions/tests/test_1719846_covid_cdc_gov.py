import pytest

URL = "https://covid.cdc.gov/covid-data-tracker/#pandemic-vulnerability-index"


IFRAME_CSS = "#pviIframe"
UNSUPPORTED_TEXT = "not support Firefox"


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL)
    assert client.await_css(IFRAME_CSS)


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL)
    assert client.await_text(UNSUPPORTED_TEXT)
