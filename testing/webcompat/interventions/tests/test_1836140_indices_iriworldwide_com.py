import pytest

URL = "https://indices.iriworldwide.com/covid19/?i=0"
BAD_MSG = "html container not found"
GOOD_CSS = "#overviewBtn"


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL)
    assert client.await_css(GOOD_CSS)


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL, wait=None, await_console_message=BAD_MSG)
