import pytest

URL = "https://www.otsuka.co.jp/fib/"
SPLASH_CSS = ".splash"
ERROR_MSG = "__im_uid_11310 is not defined"


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL)
    assert client.await_css(SPLASH_CSS)
    client.await_element_hidden(client.css(SPLASH_CSS), timeout=10)


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL, await_console_message=ERROR_MSG)
    assert client.find_css(SPLASH_CSS, is_displayed=False)
