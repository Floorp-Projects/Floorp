import pytest

URL = "https://nicochannel.jp/engage-kiss/video/smBAc4UhdTUivpbuzBexSa9d"
BLOCKED_TEXT = "このブラウザはサポートされていません。"
PLAY_BUTTON_CSS = ".nfcp-overlay-play-lg"


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL)
    assert client.await_css(PLAY_BUTTON_CSS, timeout=30)


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL)
    assert client.await_text(BLOCKED_TEXT, timeout=20)
