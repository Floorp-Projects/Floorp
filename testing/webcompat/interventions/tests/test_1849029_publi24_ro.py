import pytest

URL = "https://www.publi24.ro/anunturi/imobiliare/"
MOBILE_CSS = ".filter.mobile"
DESKTOP_CSS = ".filter.desktop"
BLOCKED_CSS = "[data-translate='block_headline']"


async def load_page(client):
    await client.navigate(URL, wait="complete")
    if client.find_css(BLOCKED_CSS):
        pytest.xfail("Site has blocked us, please test manually")


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await load_page(client)
    assert client.await_css(MOBILE_CSS)


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await load_page(client)
    assert client.await_css(DESKTOP_CSS)
