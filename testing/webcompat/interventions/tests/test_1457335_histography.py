import pytest


URL = "http://histography.io/"


@pytest.mark.with_interventions
def test_enabled(session):
    session.get(URL)
    assert session.current_url == URL


@pytest.mark.without_interventions
def test_disabled(session):
    session.get(URL)
    assert session.current_url == "http://histography.io/browser_support.htm"
