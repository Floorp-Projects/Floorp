import pytest
from helpers import Css, await_element


URL = (
    "https://www.zillow.com/homes/for_sale/Castle-Rock,-CO_rb/?"
    "searchQueryState=%7B%22pagination%22%3A%7B%7D%2C%22usersSearchTerm"
    "%22%3A%22Castle%20Rock%2C%20CO%22%2C%22mapBounds%22%3A%7B%22west"
    "%22%3A-104.89113437569046%2C%22east%22%3A-104.88703327810668%2C%22"
    "south%22%3A39.396847440697016%2C%22north%22%3A39.39931394177977"
    "%7D%2C%22regionSelection%22%3A%5B%7B%22regionId%22%3A23984%2C%22"
    "regionType%22%3A6%7D%5D%2C%22isMapVisible%22%3Atrue%2C%22filterState"
    "%22%3A%7B%22sort%22%3A%7B%22value%22%3A%22globalrelevanceex"
    "%22%7D%2C%22ah%22%3A%7B%22value%22%3Atrue%7D%7D%2C%22isListVisible"
    "%22%3Atrue%2C%22mapZoom%22%3A19%7D"
)

SVG_CSS = Css(".zillow-map-layer svg.full-boundary-svg")


def is_svg_element_overflow_hidden(session):
    # The bug is only triggered for large enough window sizes, as otherwise
    # different CSS is applied which already has overflow:hidden.
    session.set_window_size(1920, 1200)
    session.get(URL)
    await_element(session, SVG_CSS, timeout=20)
    return "hidden" == session.execute_script(
        f"""
        const svg = document.querySelector("{SVG_CSS.value}");
        if (!svg) {{
            return undefined;
        }}
        return window.getComputedStyle(svg).overflow;
    """
    )


@pytest.mark.with_interventions
def test_enabled(session):
    assert is_svg_element_overflow_hidden(session)


@pytest.mark.without_interventions
def test_disabled(session):
    assert not is_svg_element_overflow_hidden(session)
