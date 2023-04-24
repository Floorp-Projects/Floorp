import pytest

URL = "https://www.jp.square-enix.com/music/sem/page/FF7R/ost/"
ERROR_MSG = 'can\'t access property "slice", e.match(...) is null'
UNBLOCKED_CSS = "h1.logo.visible"


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL)
    assert client.await_css(UNBLOCKED_CSS)


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL, await_console_message=ERROR_MSG)
