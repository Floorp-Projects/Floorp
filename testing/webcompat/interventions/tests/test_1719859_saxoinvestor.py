import pytest
from helpers import Css, find_element


URL = (
    "https://www.saxoinvestor.fr/login/?adobe_mc="
    "MCORGID%3D173338B35278510F0A490D4C%2540AdobeOrg%7CTS%3D1621688498"
)


@pytest.mark.skip_platforms("linux")
@pytest.mark.with_interventions
def test_enabled(session):
    session.get(URL)
    userid = find_element(session, Css("input#field_userid"))
    password = find_element(session, Css("input#field_password"))
    submit = find_element(session, Css("input#button_login"))
    assert userid.is_displayed()
    assert password.is_displayed()
    assert submit.is_displayed()


@pytest.mark.skip_platforms("linux")
@pytest.mark.without_interventions
def test_disabled(session):
    session.get(URL)
    warning = find_element(session, Css("#browser_support_section"))
    assert warning.is_displayed()
