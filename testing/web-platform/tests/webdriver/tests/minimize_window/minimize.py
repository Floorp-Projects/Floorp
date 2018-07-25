from tests.support.asserts import assert_error, assert_success
from tests.support.inline import inline


def minimize(session):
    return session.transport.send(
        "POST", "session/{session_id}/window/minimize".format(**vars(session)))


def is_fullscreen(session):
    # At the time of writing, WebKit does not conform to the
    # Fullscreen API specification.
    #
    # Remove the prefixed fallback when
    # https://bugs.webkit.org/show_bug.cgi?id=158125 is fixed.
    return session.execute_script("""
        return !!(window.fullScreen || document.webkitIsFullScreen)
        """)


def test_no_browsing_context(session, create_window):
    session.window_handle = create_window()
    session.close()
    response = minimize(session)
    assert_error(response, "no such window")


def test_fully_exit_fullscreen(session):
    session.window.fullscreen()
    assert is_fullscreen(session) is True

    response = minimize(session)
    assert_success(response)
    assert is_fullscreen(session) is False
    assert session.execute_script("return document.hidden") is True


def test_minimize(session):
    assert not session.execute_script("return document.hidden")

    response = minimize(session)
    assert_success(response)
    assert session.execute_script("return document.hidden")


def test_payload(session):
    assert not session.execute_script("return document.hidden")

    response = minimize(session)

    assert response.status == 200
    assert isinstance(response.body["value"], dict)

    value = response.body["value"]
    assert "width" in value
    assert "height" in value
    assert "x" in value
    assert "y" in value
    assert isinstance(value["width"], int)
    assert isinstance(value["height"], int)
    assert isinstance(value["x"], int)
    assert isinstance(value["y"], int)

    assert session.execute_script("return document.hidden")


def test_minimize_twice_is_idempotent(session):
    assert not session.execute_script("return document.hidden")

    first_response = minimize(session)
    assert_success(first_response)
    assert session.execute_script("return document.hidden")

    second_response = minimize(session)
    assert_success(second_response)
    assert session.execute_script("return document.hidden")
