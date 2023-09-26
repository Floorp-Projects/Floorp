import time

import pytest

URL = "https://www.meteoam.it/it/home"
IFRAME_CSS = "#iframe_map"
SEARCH_CSS = "#mobile_rightpanel .barbtnsearch"
INPUT_CSS = "#search_bar_txt"


def getWindowHeight(client):
    return client.execute_script("return window.outerHeight")


async def doesHeightChange(client):
    await client.navigate(URL)
    client.switch_to_frame(client.await_css(IFRAME_CSS))
    initialHeight = getWindowHeight(client)
    client.await_css(SEARCH_CSS).click()
    client.await_css(INPUT_CSS, is_displayed=True).click()
    time.sleep(2)
    return getWindowHeight(client) != initialHeight


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    assert await doesHeightChange(client)


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    assert not await doesHeightChange(client)
