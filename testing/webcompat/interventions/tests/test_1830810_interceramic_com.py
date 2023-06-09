import time

import pytest

URL = "https://interceramic.com/mx_202VWME/galeria/index"
UNSUPPORTED_CSS = "#ff-modal"


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL)
    time.sleep(3)
    assert client.find_css(UNSUPPORTED_CSS, is_displayed=False)


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL)
    assert client.await_css(UNSUPPORTED_CSS, is_displayed=True)
