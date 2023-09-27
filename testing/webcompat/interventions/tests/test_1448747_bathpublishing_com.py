import pytest
from webdriver.error import NoSuchElementException

URL = "https://bathpublishing.com/products/clinical-negligence-made-clear-a-guide-for-patients-professionals"
POPUP_CSS = "button.recommendation-modal__close-button"
SELECT_CSS = "select#productSelect--product-template-option-0"


async def is_fastclick_active(client):
    async with client.ensure_fastclick_activates():
        await client.navigate(URL)

        try:
            client.soft_click(client.await_css(POPUP_CSS, timeout=3))
            client.await_element_hidden(client.css(POPUP_CSS))
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
