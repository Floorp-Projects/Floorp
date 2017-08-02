from support.asserts import assert_error, assert_dialog_handled, assert_success
from support.fixtures import create_dialog
from support.inline import inline


alert_doc = inline("<script>window.alert()</script>")


# 10.7.1 Get Window Rect

def test_get_window_rect_no_browsing_context(session, create_window):
    # Step 1
    session.window_handle = create_window()
    session.close()
    result = session.transport.send("GET", "session/%s/window/rect" % session.session_id)

    assert_error(result, "no such window")


def test_get_window_rect_prompt_accept(new_session):
    # Step 2
    _, session = new_session({"alwaysMatch": {"unhandledPromptBehavior": "accept"}})
    session.url = inline("<title>WD doc title</title>")

    create_dialog(session)("alert", text="dismiss #1", result_var="dismiss1")
    result = session.transport.send("GET",
                                    "session/%s/window/rect" % session.session_id)
    assert result.status == 200
    assert_dialog_handled(session, "dismiss #1")

    create_dialog(session)("confirm", text="dismiss #2", result_var="dismiss2")
    result = session.transport.send("GET",
                                    "session/%s/window/rect" % session.session_id)
    assert result.status == 200
    assert_dialog_handled(session, "dismiss #2")

    create_dialog(session)("prompt", text="dismiss #3", result_var="dismiss3")
    result = session.transport.send("GET",
                                    "session/%s/window/rect" % session.session_id)
    assert result.status == 200
    assert_dialog_handled(session, "dismiss #3")


def test_get_window_rect_handle_prompt_missing_value(session, create_dialog):
    # Step 2
    session.url = inline("<title>WD doc title</title>")
    create_dialog("alert", text="dismiss #1", result_var="dismiss1")

    result = session.transport.send("GET",
                                    "session/%s/window/rect" % session.session_id)

    assert_error(result, "unexpected alert open")
    assert_dialog_handled(session, "dismiss #1")

    create_dialog("confirm", text="dismiss #2", result_var="dismiss2")

    result = session.transport.send("GET",
                                    "session/%s/window/rect" % session.session_id)

    assert_error(result, "unexpected alert open")
    assert_dialog_handled(session, "dismiss #2")

    create_dialog("prompt", text="dismiss #3", result_var="dismiss3")

    result = session.transport.send("GET",
                                    "session/%s/window/rect" % session.session_id)

    assert_error(result, "unexpected alert open")
    assert_dialog_handled(session, "dismiss #3")


def test_get_window_rect_payload(session):
    # step 3
    result = session.transport.send("GET", "session/%s/window/rect" % session.session_id)

    assert result.status == 200
    assert isinstance(result.body["value"], dict)
    assert "width" in result.body["value"]
    assert "height" in result.body["value"]
    assert "x" in result.body["value"]
    assert "y" in result.body["value"]
    assert isinstance(result.body["value"]["width"], (int, float))
    assert isinstance(result.body["value"]["height"], (int, float))
    assert isinstance(result.body["value"]["x"], (int, float))
    assert isinstance(result.body["value"]["y"], (int, float))
