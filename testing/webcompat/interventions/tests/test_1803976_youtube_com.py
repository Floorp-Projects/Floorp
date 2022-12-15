import time

import pytest
from helpers import Css, await_element, find_element

URL = "https://www.youtube.com/"
SEARCH_TERM = "movies"

SEARCH_FIELD_CSS = Css("input[type='text'][id*='search']")
SEARCH_BUTTON_CSS = Css("#search-icon-legacy")
FILTER_BAR_CSS = Css("#filter-menu")
PERF_NOW_CONSTANT = 11111


def change_performance_now(session):
    return session.execute_script(
        f"""
        Object.defineProperty(window.performance, "now", {{
          value: function() {{
            return {PERF_NOW_CONSTANT};
          }}
      }});
    """
    )


def search_for_term(session):
    session.get(URL)
    search = await_element(session, SEARCH_FIELD_CSS)
    button = find_element(session, SEARCH_BUTTON_CSS)

    assert search.is_displayed()
    assert button.is_displayed()

    search.send_keys(SEARCH_TERM)
    time.sleep(1)
    button.click()
    time.sleep(2)


@pytest.mark.with_interventions
def test_enabled(session):
    search_for_term(session)
    filter_bar = await_element(session, FILTER_BAR_CSS, None, True)

    session.back()

    assert filter_bar.is_displayed() is False


@pytest.mark.skip(reason="results are intermittent, so skipping this test for now")
@pytest.mark.without_interventions
def test_disabled(session):
    change_performance_now(session)
    search_for_term(session)

    session.back()

    filter_bar = await_element(session, FILTER_BAR_CSS)
    assert filter_bar.is_displayed()
