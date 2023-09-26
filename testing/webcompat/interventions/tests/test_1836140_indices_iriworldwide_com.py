import pytest

URL = "https://indices.iriworldwide.com/covid19/register.html"
BAD_CSS = ".hiddPage"


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL, wait="complete")
    assert not client.find_css(BAD_CSS)


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL, wait="complete")
    assert client.find_css(BAD_CSS)
