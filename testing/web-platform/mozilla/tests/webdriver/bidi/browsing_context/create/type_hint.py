import pytest
from tests.support.asserts import assert_success

from . import using_context

pytestmark = pytest.mark.asyncio


def count_window_handles(session):
    with using_context(session, "chrome"):
        response = session.transport.send(
            "GET", "session/{session_id}/window/handles".format(**vars(session))
        )
        chrome_handles = assert_success(response)
        return len(chrome_handles)


@pytest.mark.parametrize("type_hint", ["tab", "window"])
async def test_type_hint(bidi_session, current_session, type_hint):
    assert len(await bidi_session.browsing_context.get_tree()) == 1
    assert count_window_handles(current_session) == 1

    await bidi_session.browsing_context.create(type_hint=type_hint)

    if type_hint == "window":
        expected_window_count = 2
    else:
        expected_window_count = 1

    assert len(await bidi_session.browsing_context.get_tree()) == 2
    assert count_window_handles(current_session) == expected_window_count
