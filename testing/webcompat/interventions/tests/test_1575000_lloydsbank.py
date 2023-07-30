import pytest

URL = (
    "https://apply.lloydsbank.co.uk/sales-content/cwa/l/pca/index-app.html"
    "?product=classicaccountLTB#!b"
)

NO_CSS = "[data-selector='existingCustomer-toggle-button-no']"
CONT_CSS = "[data-selector='ib-yes-continue-without-login-not-existing-customer-continue-button']"
CONT2_CSS = "[data-selector='beforeYouStart-continue-button']"
RADIO_CSS = "[name='aboutYou-gender-radio'] + span"
ACCEPT_COOKIES_CSS = "#lbganalyticsCookies [title*='Accept cookies']"


async def get_radio_position(client):
    await client.navigate(URL, wait="load")
    accept = client.await_css(ACCEPT_COOKIES_CSS, timeout=2)
    if accept:
        accept.click()
    client.await_css(NO_CSS).click()
    client.await_css(CONT_CSS).click()
    client.await_css(CONT2_CSS).click()
    radio = client.await_css(RADIO_CSS)
    return client.execute_script(
        """
        return window.getComputedStyle(arguments[0]).position;
    """,
        radio,
    )


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    assert "relative" == await get_radio_position(client)


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    assert "relative" != await get_radio_position(client)
