import pytest
from helpers import Css, expect_alert, find_element

URL = "https://ib.absa.co.za/absa-online/login.jsp"


@pytest.mark.with_interventions
def test_enabled(session):
    session.get(URL)
    assert find_element(session, Css("html.gecko"))


@pytest.mark.without_interventions
def test_disabled(session):
    session.get(URL)
    expect_alert(session, text="Browser unsupported")
    assert find_element(session, Css("html.unknown"))
