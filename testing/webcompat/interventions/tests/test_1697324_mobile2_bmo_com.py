import pytest

URL = "https://mobile2.bmo.com/"
BLOCKED_TEXT = "This browser is out-of-date"
UNBLOCKED_CSS = "auth-field"


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
