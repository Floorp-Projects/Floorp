import pytest
from helpers import Css, await_element
from selenium.common.exceptions import TimeoutException

URL = "https://www.livescience.com/"
TEXT_TO_TEST = ".trending__link"


def is_text_visible(session):
    # the page does not properly load, so we just time out
    # and wait for the element we're interested in to appear
    session.set_page_load_timeout(1)
    try:
        session.get(URL)
    except TimeoutException:
        pass
    assert await_element(session, Css(TEXT_TO_TEST))
    return session.execute_async_script(
        f"""
        const cb = arguments[0];
        const link = document.querySelector("{TEXT_TO_TEST}");
        const fullHeight = link.scrollHeight;
        const parentVisibleHeight = link.parentElement.clientHeight;
        link.style.paddingBottom = "0";
        window.requestAnimationFrame(() => {{
            const bottomPaddingHeight = fullHeight - link.scrollHeight;
            cb(fullHeight - parentVisibleHeight <= bottomPaddingHeight);
        }});
    """
    )


@pytest.mark.skip_platforms("mac")
@pytest.mark.with_interventions
def test_enabled(session):
    assert is_text_visible(session)


@pytest.mark.skip_platforms("mac")
@pytest.mark.without_interventions
def test_disabled(session):
    assert not is_text_visible(session)
