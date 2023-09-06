import pytest

URL = "https://steamcommunity.com/chat"


USERID_CSS = "input[type='text'][class*='newlogindialog']"
PASSWORD_CSS = "input[type='password'][class*='newlogindialog']"
SIGNIN_CSS = "button[type='submit'][class*='newlogindialog']"
GEAR_CSS = ".friendListButton"
LOGIN_FAIL_XPATH = (
    "//*[contains(text(), 'try again') and " "contains(@class, 'FormError')]"
)
AUTH_CSS = "[class*='newlogindialog_ProtectingAccount']"
AUTH_DIGITS_CSS = "[class*='segmentedinputs_SegmentedCharacterInput'] input.Focusable"
AUTH_RETRY_CSS = "[class*='newlogindialog_FormError']"
AUTH_RETRY_LOADER_CSS = "[class*='newlogindialog_Loading']"
RATE_TEXT = "too many login failures"
VOICE_XPATH = (
    "//*[contains(text(), 'Voice') and "
    "contains(@class, 'pagedsettings_PagedSettingsDialog_PageListItem')]"
)
MIC_BUTTON_CSS = "button.LocalMicTestButton"
UNSUPPORTED_TEXT = "currently unsupported in Firefox"


async def do_2fa(client):
    digits = client.await_css(AUTH_DIGITS_CSS, all=True)
    assert len(digits) > 0

    loader = client.css(AUTH_RETRY_LOADER_CSS)
    if client.find_element(loader):
        client.await_element_hidden(loader)
    for digit in digits:
        if digit.property("value"):
            digit.send_keys("\ue003")  # backspace

    code = input("**** Enter two-factor authentication code: ")
    for i, digit in enumerate(code):
        if len(digits) > i:
            digits[i].send_keys(digit)

    client.await_element(loader, timeout=10)
    client.await_element_hidden(loader)


async def load_mic_test(client, credentials, should_do_2fa):
    await client.navigate(URL)

    async def login():
        userid = client.await_css(USERID_CSS)
        password = client.find_css(PASSWORD_CSS)
        submit = client.find_css(SIGNIN_CSS)
        assert client.is_displayed(userid)
        assert client.is_displayed(password)
        assert client.is_displayed(submit)

        userid.send_keys(credentials["username"])
        password.send_keys(credentials["password"])
        submit.click()

    await login()

    while True:
        auth, retry, gear, fail, rate = client.await_first_element_of(
            [
                client.css(AUTH_CSS),
                client.css(AUTH_RETRY_CSS),
                client.css(GEAR_CSS),
                client.xpath(LOGIN_FAIL_XPATH),
                client.text(RATE_TEXT),
            ],
            is_displayed=True,
            timeout=20,
        )

        if retry:
            await do_2fa(client)
            continue

        if rate:
            pytest.skip(
                "Too many Steam login attempts in a short time; try again later."
            )
            return None
        elif auth:
            if should_do_2fa:
                await do_2fa(client)
                continue

            pytest.skip(
                "Two-factor authentication requested; disable Steam Guard"
                " or run this test with --do-2fa to live-input codes"
            )
            return None
        elif fail:
            pytest.skip("Invalid login provided.")
            return None
        else:
            break

    assert gear
    gear.click()

    voice = client.await_xpath(VOICE_XPATH, is_displayed=True)
    voice.click()

    mic_test = client.await_css(MIC_BUTTON_CSS, is_displayed=True)
    return mic_test


@pytest.mark.skip_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client, credentials, should_do_2fa):
    mic_test = await load_mic_test(client, credentials, should_do_2fa)
    if not mic_test:
        return

    with client.assert_getUserMedia_called():
        mic_test.click()

    assert not client.find_text(UNSUPPORTED_TEXT)
