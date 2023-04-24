import pytest

URL = "https://secure.viewer.zmags.com/services/htmlviewer/content/ddc3f3b9?pubVersion=46&locale=en&viewerID=85cfd72f#/ddc3f3b9/1"
BLOCKED_TEXT = "This browser is not supported"
UNBLOCKED_CSS = "#viewer .MainViewEnabled"


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
    await client.navigate(URL)
    assert client.await_text(BLOCKED_TEXT)
