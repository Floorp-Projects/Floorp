import pytest
from helpers import Css, is_float_cleared, find_element


URL = "https://curriculum.gov.bc.ca/curriculum/arts-education/10/media-arts"


SHOULD_CLEAR = Css(".curriculum_big_ideas")
SHOULD_BE_CLEARED = Css(".view-display-id-attachment_1")


@pytest.mark.with_interventions
def test_enabled(session):
    session.get(URL)
    should_clear = find_element(session, SHOULD_CLEAR)
    should_be_cleared = find_element(session, SHOULD_BE_CLEARED)
    assert is_float_cleared(session, should_clear, should_be_cleared)


@pytest.mark.without_interventions
def test_disabled(session):
    session.get(URL)
    should_clear = find_element(session, SHOULD_CLEAR)
    should_be_cleared = find_element(session, SHOULD_BE_CLEARED)
    assert not is_float_cleared(session, should_clear, should_be_cleared)
