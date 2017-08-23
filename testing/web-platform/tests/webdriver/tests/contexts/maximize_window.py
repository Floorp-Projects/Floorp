from tests.support.inline import inline
from tests.support.asserts import assert_error, assert_success


alert_doc = inline("<script>window.alert()</script>")


def maximize(session):
    return session.transport.send("POST", "session/%s/window/maximize" % session.session_id)


# 10.7.3 Maximize Window


def test_no_browsing_context(session, create_window):
    # step 1
    session.window_handle = create_window()
    session.close()
    response = maximize(session)
    assert_error(response, "no such window")


def test_handle_user_prompt(session):
    # step 2
    session.url = alert_doc
    response = maximize(session)
    assert_error(response, "unexpected alert open")


def test_maximize(session):
    before_size = session.window.size
    assert session.window.state == "normal"

    # step 4
    response = maximize(session)
    assert_success(response)

    assert before_size != session.window.size
    assert session.window.state == "maximized"


def test_payload(session):
    before_size = session.window.size
    assert session.window.state == "normal"

    response = maximize(session)

    # step 5
    assert response.status == 200
    assert isinstance(response.body["value"], dict)

    value = response.body["value"]
    assert "width" in value
    assert "height" in value
    assert "x" in value
    assert "y" in value
    assert "state" in value
    assert isinstance(value["width"], (int, float))
    assert isinstance(value["height"], (int, float))
    assert isinstance(value["x"], (int, float))
    assert isinstance(value["y"], (int, float))
    assert isinstance(value["state"], basestring)

    assert before_size != session.window.size
    assert session.window.state == "maximized"


def test_maximize_twice_is_idempotent(session):
    assert session.window.state == "normal"

    first_response = maximize(session)
    assert_success(first_response)
    assert session.window.state == "maximized"

    second_response = maximize(session)
    assert_success(second_response)
    assert session.window.state == "maximized"


def test_maximize_when_resized_to_max_size(session):
    # Determine the largest available window size by first maximising
    # the window and getting the window rect dimensions.
    #
    # Then resize the window to the maximum available size.
    session.end()
    assert session.window.state == "normal"
    available = session.window.maximize()
    assert session.window.state == "maximized"
    session.end()

    session.window.size = (int(available["width"]), int(available["height"]))

    # In certain window managers a window extending to the full available
    # dimensions of the screen may not imply that the window is maximised,
    # since this is often a special state.  If a remote end expects a DOM
    # resize event, this may not fire if the window has already reached
    # its expected dimensions.
    assert session.window.state == "normal"
    before = session.window.size
    session.window.maximize()
    assert session.window.size == before
    assert session.window.state == "maximized"
