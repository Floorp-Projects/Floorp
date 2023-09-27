import pytest

URL = "https://www.otsuka.co.jp/fib/"
SPLASH_CSS = ".splash"

# Note this site can be really slow


async def splash_is_hidden(client):
    await client.navigate(URL, wait="none")
    splash = client.await_css(SPLASH_CSS, is_displayed=True, timeout=20)
    assert splash
    client.await_element_hidden(client.css(SPLASH_CSS), timeout=20)
    return not client.is_displayed(splash)


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    assert await splash_is_hidden(client)


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    assert not await splash_is_hidden(client)
