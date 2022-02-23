import pytest


URL = "https://buskocchi.desuca.co.jp/smartPhone.html"
SCRIPT = """return document.getElementById("map_canvas")?.clientHeight;"""


@pytest.mark.with_interventions
def test_enabled(session):
    session.get(URL)
    assert session.execute_script(SCRIPT)


@pytest.mark.without_interventions
def test_disabled(session):
    session.get(URL)
    assert session.execute_script(SCRIPT)
