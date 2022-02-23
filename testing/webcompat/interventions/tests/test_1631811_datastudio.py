import pytest
from helpers import (
    Css,
    Text,
    await_element,
    load_page_and_wait_for_iframe,
    find_element,
)

# Note that we have to load this page twice, as when it is loaded the
# first time by the tests, the webcompat issue is avoided somehow
# (presumably a script race condition somewhere). However, it is
# triggered consistently the second time it is loaded.


URL = "https://ageor.dipot.com/2020/03/Covid-19-in-Greece.html"
FRAME_SELECTOR = Css("iframe[src^='https://datastudio.google.com/']")
FRAME_TEXT = Text("Coranavirus SARS-CoV-2 (Covid-19) in Greece")


@pytest.mark.with_interventions
def test_enabled(session):
    load_page_and_wait_for_iframe(session, URL, FRAME_SELECTOR, loads=2)
    assert session.execute_script("return window.indexedDB === undefined;")
    success_text = await_element(session, FRAME_TEXT)
    assert success_text.is_displayed()


@pytest.mark.without_interventions
def test_disabled(session):
    load_page_and_wait_for_iframe(session, URL, FRAME_SELECTOR, loads=2)
    assert session.execute_script(
        """
        try {
            window.indexedDB;
            return false;
        } catch (_) {
            return true;
        }
    """
    )
    assert find_element(session, FRAME_TEXT, default=None) is None
