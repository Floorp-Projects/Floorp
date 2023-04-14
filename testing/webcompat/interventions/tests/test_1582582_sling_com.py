import pytest

URL = "https://watch.sling.com/"
INCOMPATIBLE_CSS = "[class*='unsupported-browser']"
LOADER_CSS = ".loader-container"
VPN_TEXT = "ONLY AVAILABLE INSIDE THE US"


@pytest.mark.skip_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL)
    loader, vpn = client.await_first_element_of(
        [client.css(LOADER_CSS), client.text(VPN_TEXT)], timeout=20
    )
    assert loader or vpn
    assert not client.find_css(INCOMPATIBLE_CSS)


@pytest.mark.skip_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL)
    assert client.await_css(INCOMPATIBLE_CSS, timeout=2000)
