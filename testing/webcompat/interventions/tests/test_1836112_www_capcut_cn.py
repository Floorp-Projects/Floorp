import pytest

URL = "https://www.slushy.com/"
UNSUPPORTED_TEXT = "If you wanna see this content"
LOGIN_TEXT = "you must be at least 18"


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL)
    assert client.await_text(LOGIN_TEXT)


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL)
    assert client.await_text(UNSUPPORTED_TEXT)
