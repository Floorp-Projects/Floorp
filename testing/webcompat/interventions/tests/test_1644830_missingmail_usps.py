import pytest

URL = (
    "https://missingmail.usps.com/?_gl=1*veidlp*_gcl_aw*R0NMLjE1OTE3MjUyNTkuRUF"
    "JYUlRb2JDaE1Jd3AzaTBhYjE2UUlWa01EQUNoMlBBUVlrRUFBWUFTQUFFZ0lMeFBEX0J3RQ..*"
    "_gcl_dc*R0NMLjE1OTE3MjUyNTkuRUFJYUlRb2JDaE1Jd3AzaTBhYjE2UUlWa01EQUNoMlBBUV"
    "lrRUFBWUFTQUFFZ0lMeFBEX0J3RQ..#/"
)

USERNAME_CSS = "#username"
PASSWORD_CSS = "#password"
SIGN_IN_CSS = "#btn-submit"
TERMS_CHECKBOX_CSS = "#tc-checkbox"
TERMS_FAUX_CHECKBOX_CSS = "#tc-checkbox + .mrc-custom-checkbox"

# The USPS missing mail website takes a very long time to load (multiple
# minutes). We give them a very generous amount of time here, but will
# give up after that and just skip the rest of the test.
TIMEOUT = 900
TIMEOUT_MESSAGE = "USPS website is too slow, skipping test"


async def are_checkboxes_clickable(client, credentials):
    await client.navigate(URL)

    username = client.await_css(USERNAME_CSS)
    password = client.find_css(PASSWORD_CSS)
    sign_in = client.find_css(SIGN_IN_CSS)
    assert client.is_displayed(username)
    assert client.is_displayed(password)
    assert client.is_displayed(sign_in)

    username.send_keys(credentials["username"])
    password.send_keys(credentials["password"])
    sign_in.click()

    tc = client.await_css(TERMS_CHECKBOX_CSS, timeout=TIMEOUT)
    if tc is None:
        pytest.skip(TIMEOUT_MESSAGE)
        return

    assert not tc.selected

    # we need to simulate a real click on the checkbox
    tfc = client.find_css(TERMS_FAUX_CHECKBOX_CSS)
    await client.dom_ready()
    client.execute_script("arguments[0].scrollIntoView(true)", tfc)
    client.mouse.click(tfc).perform()
    return tc.selected


@pytest.mark.skip_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client, credentials):
    assert await are_checkboxes_clickable(client, credentials)


@pytest.mark.skip_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client, credentials):
    assert not await are_checkboxes_clickable(client, credentials)
