import pytest
from helpers import Css, await_element

URL = (
    "https://watch.antennaplus.gr/#/shows/agries_melisses/seasons/"
    "3/episode/agries_melisses_S03_E137_v1"
)


@pytest.mark.with_interventions
def test_enabled(session):
    session.get(URL)
    assert await_element(session, Css(".login-pf-page"))


@pytest.mark.without_interventions
def test_disabled(session):
    session.get(URL)
    assert await_element(session, Css(".ua-barrier"))
