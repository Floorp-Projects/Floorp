import pytest

URL = "https://watch.tonton.com.my/#/"
UNSUPPORTED_CSS = ".ua-barrier"
LOGIN_CSS = ".login-page-container"


# Skip Android as the site blocks many Android devices including the emulator
@pytest.mark.skip_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL)
    assert client.await_css(LOGIN_CSS)


@pytest.mark.skip_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL)
    assert client.await_css(UNSUPPORTED_CSS)
