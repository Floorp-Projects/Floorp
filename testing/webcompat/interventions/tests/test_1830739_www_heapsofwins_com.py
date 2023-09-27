import time

import pytest
from webdriver.error import NoSuchElementException

URL = "https://www.heapsofwins.com/games/all"
RUN_CSS = "#pagehero.games a[href*='games?play']"
IFRAME_CSS = "#gamemodal iframe"
GOOD_MSG = "GameViewModel"
BAD_MSG = "UnsupportedDevice"


async def check_for_message(client, message):
    await client.navigate(URL)
    time.sleep(1)
    try:
        client.soft_click(client.await_css(RUN_CSS))
        client.switch_to_frame(client.await_css(IFRAME_CSS))
        await (await client.promise_console_message_listener(message, timeout=30))
    except NoSuchElementException:
        pytest.xfail("Cannot test; all games seem to require login right now")


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await check_for_message(client, GOOD_MSG)


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await check_for_message(client, BAD_MSG)
