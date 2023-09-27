import pytest
from webdriver.error import NoSuchElementException

URL = "https://www.co2meter.com/collections/restaurants-food"
ITEM_CSS = "a.grid__image"
ADD_CSS = "#AddToCart"
SELECT_CSS = "select#address_country"
POPUP_CLOSE_CSS = "button.needsclick.klaviyo-close-form"


async def is_fastclick_active(client):
    async with client.ensure_fastclick_activates():
        await client.navigate(URL)

    client.soft_click(client.await_css(ITEM_CSS))
    client.soft_click(client.await_css(ADD_CSS))

    try:
        popup_close_finder = client.css(POPUP_CLOSE_CSS)
        popup_close = client.await_element(popup_close_finder, timeout=5)
        if popup_close:
            client.soft_click(popup_close)
            client.await_element_hidden(popup_close_finder)
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
