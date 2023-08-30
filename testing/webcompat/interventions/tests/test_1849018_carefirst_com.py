import pytest

URL = "https://www.carefirst.com/myaccount"
BLOCKED_TEXT = "Application Blocked"
UNBLOCKED_CSS = "#maincontent"


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL)
    assert client.await_css(UNBLOCKED_CSS)


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL)
    assert client.await_text(BLOCKED_TEXT)
