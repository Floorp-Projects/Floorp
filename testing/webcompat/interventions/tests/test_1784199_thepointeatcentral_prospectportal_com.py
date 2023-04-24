import pytest

URL = "https://thepointeatcentral.prospectportal.com/Apartments/module/application_authentication?_ga=2.250831814.1585158328.1588776830-1793033216.1588776830"
UNSUPPORTED_CSS = "[aria-label='Your browser is not supported']"


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL)
    unsupported = client.await_css(UNSUPPORTED_CSS)
    assert not client.is_displayed(unsupported)


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL)
    unsupported = client.await_css(UNSUPPORTED_CSS)
    assert client.is_displayed(unsupported)
