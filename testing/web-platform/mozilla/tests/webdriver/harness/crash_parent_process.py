import pytest


@pytest.mark.capabilities({"pageLoadStrategy": "none"})
def test_detect_crash(session):
    session.url = "about:crashparent"
