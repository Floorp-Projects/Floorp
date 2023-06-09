import pytest

URL = "https://casinoextreme.eu/games"
RUN_CSS = "a.playgame-demo[onclick^='playGame']"
IFRAME_CSS = "#gameplay > iframe"
GOOD_MSG = "GameViewModel"
BAD_MSG = "UnsupportedDevice"


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL)
    client.soft_click(client.await_css(RUN_CSS))
    client.switch_to_frame(client.await_css(IFRAME_CSS))
    await client.promise_console_message(GOOD_MSG)


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL)
    client.soft_click(client.await_css(RUN_CSS))
    client.switch_to_frame(client.await_css(IFRAME_CSS))
    await client.promise_console_message(BAD_MSG)
