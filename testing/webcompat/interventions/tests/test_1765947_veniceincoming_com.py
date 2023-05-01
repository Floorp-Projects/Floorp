import pytest
from webdriver.error import NoSuchElementException

URL = "https://veniceincoming.com/it_IT/search_results?page=1&start_date_start=&start_date_end=&k="
COOKIES_CSS = "[aria-label='cookieconsent'] .cc-allow"
IMG_CSS = ".tour-details"
TOUR_DATA_CSS = "#tour_data"


async def check_filter_opens(client):
    await client.navigate(URL)

    try:
        cookies = client.await_css(COOKIES_CSS, is_displayed=True, timeout=5)
        client.soft_click(cookies)
        client.await_element_hidden(client.css(COOKIES_CSS))
    except NoSuchElementException:
        pass

    img = client.await_css(IMG_CSS)
    client.scroll_into_view(img)
    client.mouse.click(element=img).perform()

    try:
        client.await_css(TOUR_DATA_CSS, is_displayed=True, timeout=5)
    except NoSuchElementException:
        return False
    return True


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    assert await check_filter_opens(client)


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    assert not await check_filter_opens(client)
