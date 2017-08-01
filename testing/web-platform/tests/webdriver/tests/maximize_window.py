from tests.support.inline import inline
from tests.support.asserts import assert_error, assert_success

alert_doc = inline("<script>window.alert()</script>")

# 10.7.3 Maximize Window
def test_maximize_no_browsing_context(session, create_window):
    # Step 1
    session.window_handle = create_window()
    session.close()
    result = session.transport.send("POST", "session/%s/window/maximize" % session.session_id)
    assert_error(result, "no such window")


def test_handle_user_prompt(session):
    # Step 2
    session.url = alert_doc
    result = session.transport.send("POST", "session/%s/window/maximize" % session.session_id)
    assert_error(result, "unexpected alert open")


def test_maximize(session):
    before = session.window.size

    # step 4
    result = session.transport.send("POST", "session/%s/window/maximize" % session.session_id)
    assert_success(result)

    after = session.window.size
    assert before != after


def test_payload(session):
    before = session.window.size

    result = session.transport.send("POST", "session/%s/window/maximize" % session.session_id)

    # step 5
    assert result.status == 200
    assert isinstance(result.body["value"], dict)

    rect = result.body["value"]
    assert "width" in rect
    assert "height" in rect
    assert "x" in rect
    assert "y" in rect
    assert isinstance(rect["width"], float)
    assert isinstance(rect["height"], float)
    assert isinstance(rect["x"], float)
    assert isinstance(rect["y"], float)

    after = session.window.size
    assert before != after


def test_maximize_when_resized_to_max_size(session):
    # Determine the largest available window size by first maximising
    # the window and getting the window rect dimensions.
    #
    # Then resize the window to the maximum available size.
    session.end()
    available = session.window.maximize()
    session.end()

    session.window.size = (int(available["width"]), int(available["height"]))

    # In certain window managers a window extending to the full available
    # dimensions of the screen may not imply that the window is maximised,
    # since this is often a special state.  If a remote end expects a DOM
    # resize event, this may not fire if the window has already reached
    # its expected dimensions.
    before = session.window.size
    session.window.maximize()
    after = session.window.size
    assert before == after
