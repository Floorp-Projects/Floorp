import asyncio

import pytest

URL = "https://www.planet7casino.com/lobby/?play=alien-wins"
IFRAME_CSS = "#gameiframe"
GOOD_MSG = "GameViewModel"
BAD_MSG = "UnsupportedDevice"
DOWN_TEXT = "temporarily out of order"


async def check_for_message(client, message):
    await client.navigate(URL)
    client.switch_to_frame(client.await_css(IFRAME_CSS))
    down = client.async_await_element(client.text(DOWN_TEXT))
    good = await client.promise_console_message_listener(message)
    down, good = await asyncio.wait([down, good], return_when=asyncio.FIRST_COMPLETED)
    if down:
        pytest.skip("Cannot test: casino is 'temporarily out of order'")
    return good


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    assert check_for_message(client, GOOD_MSG)


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    assert check_for_message(client, BAD_MSG)
