import pytest
from webdriver.error import NoSuchElementException

URL = "https://thehawksmoor.com/book-a-table/?location=11335"
POPUP_CSS = ".pum-overlay button.pum-close"
SELECT_CSS = "select#booktable_restaurants"


async def is_fastclick_active(client):
    async with client.ensure_fastclick_activates():
        await client.navigate(URL)

        try:
            client.soft_click(client.await_css(POPUP_CSS, timeout=3))
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
