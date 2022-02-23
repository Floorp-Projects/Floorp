import pytest
from helpers import Css, Text, find_element


def load_site(session):
    session.get("https://www.mobilesuica.com/")

    address = find_element(session, Css("input[name=MailAddress]"), default=None)
    password = find_element(session, Css("input[name=Password]"), default=None)
    error = find_element(session, Css("input[name=winclosebutton]"), default=None)

    # The page can be down at certain times, making testing impossible. For instance:
    # "モバイルSuicaサービスが可能な時間は4:00～翌日2:00です。
    #  時間をお確かめの上、再度実行してください。"
    # "Mobile Suica service is available from 4:00 to 2:00 the next day.
    #  Please check the time and try again."
    site_is_down = None is not find_element(
        session, Text("時間をお確かめの上、再度実行してください。"), default=None
    )
    if site_is_down:
        pytest.xfail("Site is currently down")

    return address, password, error, site_is_down


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
