import time

import pytest
from webdriver.error import NoSuchElementException

URL = "https://www.honda.co.uk/cars/book-a-service.html#search?query=tf33bu"
COOKIES_CSS = "#onetrust-accept-btn-handler"
CHOOSE_DEALER_CSS = "a[data-analytics-template*='Make a booking']"
BOOK_ONLINE_CSS = "select#requiredService"
SITE_DOWN_CSS = ".no-results"


async def check_choose_dealer_works(client):
    await client.navigate(URL)

    cookies = client.css(COOKIES_CSS)
    client.await_element(cookies).click()
    client.await_element_hidden(cookies)
    time.sleep(0.5)

    down, dealer = client.await_first_element_of(
        [
            client.css(SITE_DOWN_CSS),
            client.css(CHOOSE_DEALER_CSS),
        ],
        timeout=5,
    )

    if down:
        pytest.skip("Service is down right now, so testing is impossible")
        return True

    client.scroll_into_view(dealer)
    client.mouse.click(element=dealer).perform()
    try:
        client.await_css(BOOK_ONLINE_CSS, is_displayed=True, timeout=3)
    except NoSuchElementException:
        return False
    return True


@pytest.mark.skip_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    assert await check_choose_dealer_works(client)


@pytest.mark.skip_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    assert not await check_choose_dealer_works(client)
