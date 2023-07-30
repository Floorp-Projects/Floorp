import pytest

URL = "http://histography.io/"
SUPPORT_URL = "http://histography.io/browser_support.htm"


@pytest.mark.skip_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL, wait="load")
    assert client.current_url == URL


@pytest.mark.skip_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL, wait="load")
    assert client.current_url == SUPPORT_URL
