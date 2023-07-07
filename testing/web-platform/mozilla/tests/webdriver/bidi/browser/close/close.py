import pytest

pytestmark = pytest.mark.asyncio


async def test_close_browser(new_session, add_browser_capabilities):
    bidi_session = await new_session(
        capabilities={"alwaysMatch": add_browser_capabilities({})}
    )

    await bidi_session.browser.close()

    # Wait for the browser to actually close.
    bidi_session.current_browser.wait()

    assert bidi_session.current_browser.is_running is False


async def test_start_session_again(new_session, add_browser_capabilities):
    bidi_session = await new_session(
        capabilities={"alwaysMatch": add_browser_capabilities({})}
    )
    first_session_id = bidi_session.session_id

    await bidi_session.browser.close()

    # Wait for the browser to actually close.
    bidi_session.current_browser.wait()

    # Try to create a session again.
    bidi_session = await new_session(
        capabilities={"alwaysMatch": add_browser_capabilities({})}
    )

    assert isinstance(bidi_session.session_id, str)
    assert first_session_id != bidi_session.session_id
