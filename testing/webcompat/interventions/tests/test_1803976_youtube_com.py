import time

import pytest

URL = "https://www.youtube.com/"
SEARCH_TERM = "movies"

SEARCH_FIELD_CSS = "input[type='text'][id*='search']"
SEARCH_BUTTON_CSS = "#search-icon-legacy"
FILTER_BAR_CSS = "#filter-menu"
PERF_NOW_CONSTANT = 11111


def change_performance_now(client):
    return client.execute_script(
        """
        const PERF_NOW = arguments[0];
        Object.defineProperty(window.performance, "now", {
          value: () => PERF_NOW,
        }, PERF_NOW_CONSTANT);
    """
    )


async def search_for_term(client):
    await client.navigate(URL)
    search = client.await_css(SEARCH_FIELD_CSS)
    button = client.find_css(SEARCH_BUTTON_CSS)

    assert client.is_displayed(search)
    assert client.is_displayed(button)

    search.send_keys(SEARCH_TERM)
    time.sleep(1)
    button.click()
    time.sleep(2)


@pytest.mark.skip_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await search_for_term(client)
    filter_bar = client.await_css(FILTER_BAR_CSS)

    client.back()

    assert not client.is_displayed(filter_bar)


@pytest.mark.skip(reason="results are intermittent, so skipping this test for now")
@pytest.mark.skip_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    change_performance_now(client)
    await search_for_term(client)

    client.back()

    filter_bar = client.await_css(FILTER_BAR_CSS)
    assert client.is_displayed(filter_bar)
