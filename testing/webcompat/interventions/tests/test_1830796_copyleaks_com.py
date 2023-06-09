import time

import pytest

URL = "https://copyleaks.com/ai-content-detector"
IFRAME_CSS = "#ai-content-detector"
UNSUPPORTED_CSS = "#outdated"


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL)
    client.switch_to_frame(client.await_css(IFRAME_CSS))
    time.sleep(2)
    assert not client.find_css(UNSUPPORTED_CSS, is_displayed=True)


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL)
    client.switch_to_frame(client.await_css(IFRAME_CSS))
    assert client.await_css(UNSUPPORTED_CSS, is_displayed=True)
