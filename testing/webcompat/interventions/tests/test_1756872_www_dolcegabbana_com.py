import pytest

URL = "https://www.dolcegabbana.com/en/"
FAIL_CSS = "#p-homepage"
SUCCESS_CSS = "#app"


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL)
    assert client.await_css(SUCCESS_CSS)
    assert not client.find_css(FAIL_CSS)


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL)
    assert client.await_css(FAIL_CSS)
    assert not client.find_css(SUCCESS_CSS)
