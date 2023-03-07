import pytest

URL = "https://book.ersthelfer.tv/"
DATE = "12.12.2020"
PLACEHOLDER = "__.__.____"

CITY_CSS = "#city_id"
PERSON_CSS = "#no_of_person"
CITY_OPTION_XPATH = "//select[@name='city_id']/option[2]"
PERSON_OPTION_XPATH = "//select[@name='no_of_person']/option[2]"
DATE_PICKER_CSS = "[class*='date-picker-custom-wrapper'] input"


async def set_date(client):
    client.await_css(CITY_CSS).click()
    client.await_xpath(CITY_OPTION_XPATH).click()

    client.await_css(PERSON_CSS).click()
    client.await_xpath(PERSON_OPTION_XPATH).click()
    date_input = client.await_css(DATE_PICKER_CSS, is_displayed=True)
    date_input.send_keys(DATE)
    return date_input.property("value")


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL)
    assert DATE == await set_date(client)


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL)
    assert PLACEHOLDER == await set_date(client)
