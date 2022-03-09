import pytest
from helpers import Css, await_element

URL = (
    "https://apply.lloydsbank.co.uk/sales-content/cwa/l/pca/index-app.html"
    "?product=classicaccountLTB#!b"
)

NO_CSS = Css("[data-selector='existingCustomer-toggle-button-no']")
CONT_CSS = Css(
    "[data-selector='ib-yes-continue-without-login-not-existing-customer-continue-button']"
)
CONT2_CSS = Css("[data-selector='beforeYouStart-continue-button']")
RADIO_CSS = Css("[name='aboutYou-gender-radio'] + span")


def get_radio_position(session):
    session.get(URL)
    await_element(session, NO_CSS).click()
    await_element(session, CONT_CSS).click()
    await_element(session, CONT2_CSS).click()
    await_element(session, RADIO_CSS)
    return session.execute_script(
        f"""
        const r = document.querySelector("{RADIO_CSS.value}");
        return window.getComputedStyle(r).position;
    """
    )


@pytest.mark.with_interventions
def test_enabled(session):
    assert "relative" == get_radio_position(session)


@pytest.mark.without_interventions
def test_disabled(session):
    assert "relative" != get_radio_position(session)
