import pytest
from webdriver.error import NoSuchElementException

URL = (
    "https://apply.lloydsbank.co.uk/sales-content/cwa/l/pca/index-app.html"
    "?product=classicaccountLTB"
)

NO_CSS = "[data-selector='existingCustomer-toggle-button-no']"
CONT_CSS = "[data-selector='ib-yes-continue-without-login-not-existing-customer-continue-button']"
CONT2_CSS = "[data-selector='beforeYouStart-continue-button']"
RADIO_CSS = "[name='aboutYou-gender-radio'] + span"
ACCEPT_COOKIES_CSS = "#lbganalyticsCookies button#accept"


async def get_radio_position(client):
    loadedProperly = False
    for i in range(0, 10):
        await client.navigate(URL, wait="none")
        try:
            loadedProperly = client.await_css(NO_CSS, timeout=3)
        except NoSuchElementException:
            continue
        accept = client.find_css(ACCEPT_COOKIES_CSS)
        if accept:
            accept.click()
        client.await_css(NO_CSS).click()
        client.await_css(NO_CSS).click()
        client.await_css(CONT_CSS).click()
        client.await_css(CONT2_CSS).click()
        break
    if not loadedProperly:
        pytest.xfail("The website seems to not be loading properly")
        return False
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
