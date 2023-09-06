import pytest

URL = "https://watch.tonton.com.my/#/"
UNSUPPORTED_CSS = ".ua-barrier"
LOGIN_CSS = ".login-page-container"


# The site can take a little time to load, and this includes
# interstitial ads, so for now we give it 10 seconds.


# Skip Android as the site blocks many Android devices including the emulator
@pytest.mark.skip_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL)
    assert client.await_css(LOGIN_CSS, timeout=10)


@pytest.mark.skip_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL)
    assert client.await_css(UNSUPPORTED_CSS, timeout=10)
