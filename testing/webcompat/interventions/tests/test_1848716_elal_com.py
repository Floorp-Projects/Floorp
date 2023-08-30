import pytest

URL = "https://www.elal.com/he/Israel/Pages/default.aspx"
UNSUPPORTED_CSS = "#upgrade-browser-wrap"


@pytest.mark.skip_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL)
    unsupported = client.await_css(UNSUPPORTED_CSS)
    assert not client.is_displayed(unsupported)


@pytest.mark.skip_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL)
    unsupported = client.await_css(UNSUPPORTED_CSS)
    assert client.is_displayed(unsupported)
