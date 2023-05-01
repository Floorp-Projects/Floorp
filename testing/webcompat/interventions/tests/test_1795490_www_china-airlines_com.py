import pytest

URL = "https://www.china-airlines.com/tw/en/booking/book-flights/flight-search?lang=en-us&deptstn=TPE&arrstn=LAX"
DATE_CSS = "#departureDateMobile"
DATE_DISABLED_CSS = "#departureDateMobile[disabled]"


async def check_date_disabled(client):
    await client.navigate(URL)
    client.await_css(DATE_CSS)
    return client.find_css(DATE_DISABLED_CSS)


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    assert not await check_date_disabled(client)


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    assert await check_date_disabled(client)
