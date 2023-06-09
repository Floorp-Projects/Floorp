import pytest

URL = "https://yabbycasino.com/game/aces-and-eights"
IFRAME_CSS = "iframe#game"
GOOD_CSS = "#rotateDevice"
BAD_CSS = ".unsupported-device-box"


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL)
    client.switch_to_frame(client.await_css(IFRAME_CSS))
    assert client.await_css(GOOD_CSS, is_displayed=True)


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL)
    client.switch_to_frame(client.await_css(IFRAME_CSS))
    assert client.await_css(BAD_CSS, is_displayed=True)
