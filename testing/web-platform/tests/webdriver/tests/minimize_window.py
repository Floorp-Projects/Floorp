from tests.support.inline import inline
from tests.support.asserts import assert_error, assert_success

alert_doc = inline("<script>window.alert()</script>")

# 10.7.4 Minimize Window
def test_minimize_no_browsing_context(session, create_window):
    # Step 1
    session.window_handle = create_window()
    session.close()
    result = session.transport.send("POST", "session/%s/window/minimize" % session.session_id)
    assert_error(result, "no such window")


def test_handle_user_prompt(session):
    # Step 2
    session.url = alert_doc
    result = session.transport.send("POST", "session/%s/window/minimize" % session.session_id)
    assert_error(result, "unexpected alert open")


def test_minimize(session):
    before_size = session.window.size
    assert session.window.state == "normal"

    # step 4
    result = session.transport.send("POST", "session/%s/window/minimize" % session.session_id)
    assert_success(result)

    assert session.window.state == "minimized"


def test_payload(session):
    before_size = session.window.size
    assert session.window.state == "normal"

    result = session.transport.send("POST", "session/%s/window/minimize" % session.session_id)

    # step 5
    assert result.status == 200
    assert isinstance(result.body["value"], dict)

    resp = result.body["value"]
    assert "width" in resp
    assert "height" in resp
    assert "x" in resp
    assert "y" in resp
    assert "state" in resp
    assert isinstance(resp["width"], (int, float))
    assert isinstance(resp["height"], (int, float))
    assert isinstance(resp["x"], (int, float))
    assert isinstance(resp["y"], (int, float))
    assert isinstance(resp["state"], basestring)

    assert session.window.state == "minimized"

