from support.inline import inline
from support.asserts import assert_error, assert_success

alert_doc = inline("<script>window.alert()</script>")

# 10.7.3 Maximize Window
def test_maximize_no_browsing_context(session, create_window):
    # Step 1
    session.window_handle = create_window()
    session.close()
    result = session.transport.send("POST", "session/%s/window/maximize" % session.session_id)

    assert_error(result, "no such window")


def test_maximize_rect_alert_prompt(session):
    # Step 2
    session.url = alert_doc

    result = session.transport.send("POST", "session/%s/window/maximize" % session.session_id)

    assert_error(result, "unexpected alert open")


def test_maximize_payload(session):
    # step 5
    result = session.transport.send("POST", "session/%s/window/maximize" % session.session_id)

    assert result.status == 200
    assert isinstance(result.body["value"], dict)
    assert "width" in result.body["value"]
    assert "height" in result.body["value"]
    assert "x" in result.body["value"]
    assert "y" in result.body["value"]
    assert isinstance(result.body["value"]["width"], float)
    assert isinstance(result.body["value"]["height"], float)
    assert isinstance(result.body["value"]["x"], float)
    assert isinstance(result.body["value"]["y"], float)
