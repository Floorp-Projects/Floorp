import pytest

ADDRESS_CSS = "input[name=MailAddress]"
PASSWORD_CSS = "input[name=Password]"
CLOSE_BUTTON_CSS = "input[name=winclosebutton]"
UNAVAILABLE_TEXT = "時間をお確かめの上、再度実行してください。"
UNSUPPORTED_TEXT = "ご利用のブラウザでは正しく"


async def load_site(client):
    await client.navigate("https://www.mobilesuica.com/")

    address = client.find_css(ADDRESS_CSS)
    password = client.find_css(PASSWORD_CSS)
    error1 = client.find_css(CLOSE_BUTTON_CSS)
    error2 = client.find_text(UNSUPPORTED_TEXT)

    # The page can be down at certain times, making testing impossible. For instance:
    # "モバイルSuicaサービスが可能な時間は4:00～翌日2:00です。
    #  時間をお確かめの上、再度実行してください。"
    # "Mobile Suica service is available from 4:00 to 2:00 the next day.
    #  Please check the time and try again."
    site_is_down = client.find_text(UNAVAILABLE_TEXT)
    if site_is_down is not None:
        pytest.xfail("Site is currently down")

    return address, password, error1 or error2, site_is_down


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    address, password, error, site_is_down = await load_site(client)
    if site_is_down:
        return
    assert client.is_displayed(address)
    assert client.is_displayed(password)
    assert error is None


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    address, password, error, site_is_down = await load_site(client)
    if site_is_down:
        return
    assert address is None
    assert password is None
    assert client.is_displayed(error)
