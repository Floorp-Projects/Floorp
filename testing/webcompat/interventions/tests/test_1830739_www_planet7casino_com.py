import pytest

URL = "https://www.planet7casino.com/lobby/?play=alien-wins"
IFRAME_CSS = "#gameiframe"
GOOD_MSG = "GameViewModel"
BAD_MSG = "UnsupportedDevice"


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL)
    client.switch_to_frame(client.await_css(IFRAME_CSS))
    await client.promise_console_message(GOOD_MSG)


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL)
    client.switch_to_frame(client.await_css(IFRAME_CSS))
    await client.promise_console_message(BAD_MSG)
