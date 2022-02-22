import pytest
from helpers import Css, await_dom_ready, await_element, find_element
from selenium.webdriver.common.action_chains import ActionChains

URL = (
    "https://missingmail.usps.com/?_gl=1*veidlp*_gcl_aw*R0NMLjE1OTE3MjUyNTkuRUF"
    "JYUlRb2JDaE1Jd3AzaTBhYjE2UUlWa01EQUNoMlBBUVlrRUFBWUFTQUFFZ0lMeFBEX0J3RQ..*"
    "_gcl_dc*R0NMLjE1OTE3MjUyNTkuRUFJYUlRb2JDaE1Jd3AzaTBhYjE2UUlWa01EQUNoMlBBUV"
    "lrRUFBWUFTQUFFZ0lMeFBEX0J3RQ..#/"
)

USERNAME_CSS = Css("#username")
PASSWORD_CSS = Css("#password")
SIGN_IN_CSS = Css("#btn-submit")
TERMS_CHECKBOX_CSS = Css("#tc-checkbox")
TERMS_FAUX_CHECKBOX_CSS = Css("#tc-checkbox + .mrc-custom-checkbox")

# The USPS missing mail website takes a very long time to load (multiple
# minutes). We give them a very generous amount of time here, but will
# give up after that and just skip the rest of the test.
TIMEOUT = 900
TIMEOUT_MESSAGE = "USPS website is too slow, skipping test"


def are_checkboxes_clickable(session, credentials):
    session.get(URL)

    username = find_element(session, USERNAME_CSS)
    password = find_element(session, PASSWORD_CSS)
    sign_in = find_element(session, SIGN_IN_CSS)
    assert username.is_displayed()
    assert password.is_displayed()
    assert sign_in.is_displayed()

    username.send_keys(credentials["username"])
    password.send_keys(credentials["password"])
    sign_in.click()

    tc = await_element(session, TERMS_CHECKBOX_CSS, timeout=TIMEOUT, default=None)
    if tc is None:
        pytest.skip(TIMEOUT_MESSAGE)
        return

    assert not tc.is_selected()
    tfc = find_element(session, TERMS_FAUX_CHECKBOX_CSS)
    await_dom_ready(session)
    session.execute_script("arguments[0].scrollIntoView(true)", tfc)

    action = ActionChains(session)
    action.move_to_element(tfc).click().perform()
    return tc.is_selected()


@pytest.mark.with_interventions
def test_enabled(session, credentials):
    assert are_checkboxes_clickable(session, credentials)


@pytest.mark.without_interventions
def test_disabled(session, credentials):
    assert not are_checkboxes_clickable(session, credentials)
