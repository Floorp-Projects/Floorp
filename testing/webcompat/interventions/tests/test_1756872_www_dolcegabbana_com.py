import pytest
from webdriver.error import NoSuchElementException

URL = "https://www.dolcegabbana.com/en/"
COOKIES_CSS = "#CybotCookiebotDialogBodyLevelButtonLevelOptinAllowAll"
FAIL_CSS = "#p-homepage"
SUCCESS_CSS = "#app"


async def load_page(client):
    await client.navigate(URL)
    try:
        client.remove_element(client.await_css(COOKIES_CSS, timeout=4))
    except NoSuchElementException:
        pass


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await load_page(client)
    assert client.await_css(SUCCESS_CSS)
    assert not client.find_css(FAIL_CSS)


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await load_page(client)
    assert client.await_css(FAIL_CSS)
    assert not client.find_css(SUCCESS_CSS)
