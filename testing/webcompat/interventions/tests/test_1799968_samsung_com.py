import pytest

URL = "https://www.samsung.com/in/watches/galaxy-watch/galaxy-watch5-44mm-graphite-bluetooth-sm-r910nzaainu/"
GOOD_CSS = "html.linux.firefox"
ERROR_MSG = "DOMTokenList.add: The empty string is not a valid token."


@pytest.mark.only_platforms("linux")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL)
    assert client.await_css(GOOD_CSS)


@pytest.mark.only_platforms("linux")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL, await_console_message=ERROR_MSG)
    assert not client.find_css(GOOD_CSS)
