from support.context import using_context
from tests.support.asserts import assert_success


def get_window_handle(session):
    return session.transport.send(
        "GET", "session/{session_id}/window".format(**vars(session))
    )


def test_basic(session):
    with using_context(session, "chrome"):
        response = get_window_handle(session)
        assert_success(response, session.window_handle)


def test_different_handle_than_content_scope(session):
    response = get_window_handle(session)
    content_handle = assert_success(response)

    with using_context(session, "chrome"):
        response = get_window_handle(session)
        chrome_handle = assert_success(response)

    assert chrome_handle != content_handle
