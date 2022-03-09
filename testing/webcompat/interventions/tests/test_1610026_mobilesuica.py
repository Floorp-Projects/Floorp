import pytest
from helpers import Css, Text, find_element


ADDRESS_CSS = Css("input[name=MailAddress]")
PASSWORD_CSS = Css("input[name=Password]")
CLOSE_BUTTON_CSS = Css("input[name=winclosebutton]")
UNAVAILABLE_TEXT = Text("時間をお確かめの上、再度実行してください。")
UNSUPPORTED_TEXT = Text("ご利用のブラウザでは正しく")


def load_site(session):
    session.get("https://www.mobilesuica.com/")

    address = find_element(session, ADDRESS_CSS, default=None)
    password = find_element(session, PASSWORD_CSS, default=None)
    error1 = find_element(session, CLOSE_BUTTON_CSS, default=None)
    error2 = find_element(session, UNSUPPORTED_TEXT, default=None)

    # The page can be down at certain times, making testing impossible. For instance:
    # "モバイルSuicaサービスが可能な時間は4:00～翌日2:00です。
    #  時間をお確かめの上、再度実行してください。"
    # "Mobile Suica service is available from 4:00 to 2:00 the next day.
    #  Please check the time and try again."
    site_is_down = find_element(session, UNAVAILABLE_TEXT, default=None)
    if site_is_down is not None:
        pytest.xfail("Site is currently down")

    return address, password, error1 or error2, site_is_down


@pytest.mark.with_interventions
def test_enabled(session):
    address, password, error, site_is_down = load_site(session)
    if site_is_down:
        return
    assert address.is_displayed()
    assert password.is_displayed()
    assert error is None


@pytest.mark.without_interventions
def test_disabled(session):
    address, password, error, site_is_down = load_site(session)
    if site_is_down:
        return
    assert address is None
    assert password is None
    assert error.is_displayed()
