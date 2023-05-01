import pytest
from webdriver.error import NoSuchElementException

URL = (
    "https://www.marksandspencer.com/webapp/wcs/stores/servlet/GenericApplicationError"
)
COOKIES_CSS = "button.navigation-cookiebbanner__submit"
SELECT_CSS = "#progressiveHeaderSection button.navigation-hamburger-trigger"


async def is_fastclick_active(client):
    await client.navigate(URL)
    try:
        client.await_css(COOKIES_CSS, timeout=3).click()
        client.await_element_hidden(client.css(COOKIES_CSS))
    except NoSuchElementException:
        pass

    return client.test_for_fastclick(client.await_css(SELECT_CSS))


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    assert not await is_fastclick_active(client)


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    assert await is_fastclick_active(client)
