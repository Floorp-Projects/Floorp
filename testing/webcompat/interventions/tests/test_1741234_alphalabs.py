import pytest

URL = "https://patient.alphalabs.ca/Report/CovidReport"

CONTINUE_CSS = ".btnNormal[type='submit']"
FOOTER_CSS = "footer"


def is_continue_above_footer(session):
    session.get(URL)
    return session.execute_script(
        f"""
        const cont = document.querySelector("{CONTINUE_CSS}").getClientRects()[0];
        const footer = document.querySelector("{FOOTER_CSS}").getClientRects()[0];
        return cont.bottom < footer.top;
    """
    )


@pytest.mark.with_interventions
def test_enabled(session):
    assert is_continue_above_footer(session)


@pytest.mark.without_interventions
def test_disabled(session):
    assert not is_continue_above_footer(session)
