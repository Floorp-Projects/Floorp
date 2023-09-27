import pytest

URL = "https://www.frontgate.com/resort-cotton-bath-towels/155771"
MOBILE_CSS = "#app"
DESKTOP_CSS = "#cbiBody"

# Note that this intervention doesn't seem to work 100% of
# the time so this test may intermittently fail.


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL)
    assert client.await_css(MOBILE_CSS, timeout=30)
    assert not client.find_css(DESKTOP_CSS)


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL)
    assert client.await_css(DESKTOP_CSS, timeout=30)
    assert not client.find_css(MOBILE_CSS)
