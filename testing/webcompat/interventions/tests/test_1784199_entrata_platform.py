import pytest

URL = "https://www.liveobserverpark.com/"
UNSUPPORTED_BANNER = ".banner_overlay"


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL)
    unsupported_banner = client.await_css(UNSUPPORTED_BANNER)
    assert not client.is_displayed(unsupported_banner)


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL)
    unsupported_banner = client.await_css(UNSUPPORTED_BANNER)
    assert client.is_displayed(unsupported_banner)
