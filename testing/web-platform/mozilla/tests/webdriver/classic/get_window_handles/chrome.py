from support.context import using_context
from tests.support.asserts import assert_success


def get_window_handles(session):
    return session.transport.send(
        "GET", "session/{session_id}/window/handles".format(**vars(session))
    )


def test_basic(session):
    with using_context(session, "chrome"):
        response = get_window_handles(session)
        assert_success(response, session.handles)


def test_different_handles_than_content_scope(session):
    response = get_window_handles(session)
    content_handles = assert_success(response)

    with using_context(session, "chrome"):
        response = get_window_handles(session)
        chrome_handles = assert_success(response)

    assert chrome_handles != content_handles
    assert len(chrome_handles) == 1
    assert len(content_handles) == 1


def test_multiple_windows_and_tabs(session):
    session.new_window(type_hint="window")
    session.new_window(type_hint="tab")

    response = get_window_handles(session)
    content_handles = assert_success(response)

    with using_context(session, "chrome"):
        response = get_window_handles(session)
        chrome_handles = assert_success(response)

    assert chrome_handles != content_handles
    assert len(chrome_handles) == 2
    assert len(content_handles) == 3
