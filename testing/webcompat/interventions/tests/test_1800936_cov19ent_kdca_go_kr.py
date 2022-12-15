import pytest

URL = "https://cov19ent.kdca.go.kr/"
HEADER_CSS = "header.mouseOn"


def get_header_position(session):
    session.get(URL)
    return session.execute_script(
        f"""
        const r = document.querySelector("{HEADER_CSS}");
        return window.getComputedStyle(r).position;
    """
    )


@pytest.mark.with_interventions
def test_enabled(session):
    assert "absolute" == get_header_position(session)


@pytest.mark.without_interventions
def test_disabled(session):
    assert "absolute" != get_header_position(session)
