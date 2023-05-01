import pytest
from webdriver.error import NoSuchElementException

URL = "https://mobilevikings.be/en/registration/?product_id=155536813-1-1"
COOKIES_CSS = "#btn-accept-cookies"
DATE_CSS = "input.date-input[name='birth_date']"


async def date_after_typing(client):
    await client.navigate(URL)
    try:
        client.await_css(COOKIES_CSS, timeout=3).click()
        client.await_element_hidden(client.css(COOKIES_CSS))
    except NoSuchElementException:
        pass
    date = client.await_css(DATE_CSS)
    client.scroll_into_view(date)
    date.send_keys("1")
    return date.property("value")


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    assert "1_/__/____" == await date_after_typing(client)


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    assert "__/__/____" == await date_after_typing(client)
