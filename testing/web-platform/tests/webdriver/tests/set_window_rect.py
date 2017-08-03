import pytest

from support.inline import inline
from support.fixtures import create_dialog
from support.asserts import assert_error, assert_dialog_handled, assert_success

alert_doc = inline("<script>window.alert()</script>")

# 10.7.2 Set Window Rect

def test_set_window_rect_prompt_accept(new_session):
    # Step 2
    _, session = new_session({"alwaysMatch": {"unhandledPromptBehavior": "accept"}})
    session.url = inline("<title>WD doc title</title>")

    get_response = session.transport.send("GET", "session/%s/window/rect" % session.session_id)
    original = get_response.body["value"]

    create_dialog(session)("alert", text="dismiss #1", result_var="dismiss1")
    result = session.transport.send("POST",
                                    "session/%s/window/rect" % session.session_id,
                                    {"x": int(original["y"]),
                                     "y": int(original["y"])})
    assert result.status == 200
    assert_dialog_handled(session, "dismiss #1")

    create_dialog(session)("confirm", text="dismiss #2", result_var="dismiss2")
    result = session.transport.send("POST",
                                    "session/%s/window/rect" % session.session_id,
                                    {"x": int(original["y"]),
                                     "y": int(original["y"])})
    assert result.status == 200
    assert_dialog_handled(session, "dismiss #2")

    create_dialog(session)("prompt", text="dismiss #3", result_var="dismiss3")
    result = session.transport.send("POST",
                                    "session/%s/window/rect" % session.session_id,
                                    {"x": int(original["y"]),
                                     "y": int(original["y"])})
    assert result.status == 200
    assert_dialog_handled(session, "dismiss #3")


def test_set_window_rect_handle_prompt_missing_value(session, create_dialog):
    # Step 2
    get_response = session.transport.send("GET",
                                          "session/%s/window/rect" % session.session_id)
    original = get_response.body["value"]

    session.url = inline("<title>WD doc title</title>")
    create_dialog("alert", text="dismiss #1", result_var="dismiss1")

    result = session.transport.send("POST",
                                    "session/%s/window/rect" % session.session_id,
                                    {"x": int(original["y"]),
                                     "y": int(original["y"])})

    assert_error(result, "unexpected alert open")
    assert_dialog_handled(session, "dismiss #1")

    create_dialog("confirm", text="dismiss #2", result_var="dismiss2")

    result = session.transport.send("POST",
                                    "session/%s/window/rect" % session.session_id,
                                    {"x": int(original["y"]),
                                     "y": int(original["y"])})

    assert_error(result, "unexpected alert open")
    assert_dialog_handled(session, "dismiss #2")

    create_dialog("prompt", text="dismiss #3", result_var="dismiss3")

    result = session.transport.send("POST",
                                    "session/%s/window/rect" % session.session_id,
                                    {"x": int(original["y"]),
                                     "y": int(original["y"])})

    assert_error(result, "unexpected alert open")
    assert_dialog_handled(session, "dismiss #3")


@pytest.mark.parametrize("data", [
    {"height": None, "width": None, "x": "a", "y": "b"},
    {"height": "a", "width": "b", "x": None, "y": None},
    {"height": None, "width": None, "x": 10.1, "y": 10.1},
    {"height": 10.1, "width": 10.1, "x": None, "y": None},
    {"height": True, "width": False, "x": None, "y": None},
    {"height": None, "width": None, "x": True, "y": False},
    {"height": [], "width": [], "x": None, "y": None},
    {"height": None, "width": None, "x": [], "y": []},
    {"height": [], "width": [], "x": [], "y": []},
    {"height": {}, "width": {}, "x": None, "y": None},
    {"height": None, "width": None, "x": {}, "y": {}},
])
def test_set_window_rect_invalid_params(session, data):
    # step 8-9
    result = session.transport.send("POST",
                                    "session/%s/window/rect" % session.session_id,
                                    data)

    assert_error(result, "invalid argument")


def test_set_window_fullscreen(session):
    get_response = session.transport.send("GET", "session/%s/window/rect" % session.session_id)
    original = get_response.body["value"]

    # step 10
    session.transport.send("POST",
                           "session/%s/window/fullscreen" % session.session_id)
    assert session.execute_script("return document.fullscreenElement != null")
    result = session.transport.send("POST",
                                    "session/%s/window/rect" % session.session_id,
                                    {"width": 400, "height": 400})
    assert session.execute_script("return document.fullscreenElement == null")
    assert_success(result, {"x": original["x"],
                            "y": original["y"],
                            "width": 400,
                            "height": 400})


def test_set_window_rect_window_minimized(session):
    # step 11
    session.minimize()
    assert session.execute_script("return document.hidden")
    result = session.transport.send("POST",
                                    "session/%s/window/rect" % session.session_id,
                                    {"width": 400, "height": 400})
    assert not session.execute_script("return document.hidden")
    assert_success(result, {"width": 400, "height": 400})


def test_set_window_height_width(session):
    get_response = session.transport.send("GET", "session/%s/window/rect" % session.session_id)
    original = get_response.body["value"]

    # Step 12
    _max = session.execute_script("""
        return {
          width: window.screen.availWidth,
          height: window.screen.availHeight,
        }""")
    result = session.transport.send("POST",
                                    "session/%s/window/rect" % session.session_id,
                                    {"width": _max["width"] - 100,
                                     "height": _max["height"] - 100})

    # Step 14
    assert_success(result, {"x": original["x"], "y": original["y"],
                            "width": _max["width"] - 100,
                            "height": _max["height"] - 100})


def test_set_window_height_width_larger_than_max(session):
    # Step 12
    _max = session.execute_script("""
        return {
          width: window.screen.availWidth,
          height: window.screen.availHeight,
        }""")
    result = session.transport.send("POST",
                                    "session/%s/window/rect" % session.session_id,
                                    {"width": _max["width"] + 100,
                                     "height": _max["width"] + 100})

    # Step 14
    assert result.status == 200
    assert result.body["value"]["width"] >= _max["width"]
    assert result.body["value"]["height"] >= _max["height"]


def test_set_window_height_width_as_current(session):
    # Step 12
    get_response = session.transport.send("GET", "session/%s/window/rect" % session.session_id)
    original = get_response.body["value"]
    result = session.transport.send("POST",
                                    "session/%s/window/rect" % session.session_id,
                                    {"width": int(original["width"]),
                                     "height": int(original["height"])})

    # Step 14
    assert_success(result, {"x": original["x"],
                            "y": original["y"],
                            "width": original["width"],
                            "height": original["height"]})


def test_set_window_rect_x_y(session):
    # Step 13
    get_response = session.transport.send("GET", "session/%s/window/rect" % session.session_id)
    original = get_response.body["value"]
    result = session.transport.send("POST",
                                    "session/%s/window/rect" % session.session_id,
                                    {"x": int(original["x"]) + 10,
                                     "y": int(original["y"]) + 10})
    # Step 14
    assert_success(result, {"x": original["x"] + 10,
                            "y": original["y"] + 10,
                            "width": original["width"],
                            "height": original["height"]})


def test_set_window_rect_negative_x_y(session):
    get_response = session.transport.send("GET", "session/%s/window/rect" % session.session_id)
    original = get_response.body["value"]

    # Step 13
    result = session.transport.send("POST",
                                    "session/%s/window/rect" % session.session_id,
                                    {"x": - 8,
                                     "y": - 8})

    os = session.capabilities["platformName"]
    # Step 14
    if os == "linux":
        # certain WMs prohibit windows from being moved off-screen
        assert_success(result, {"x": 0, "y": 0,
                                "width": original["width"],
                                "height": original["height"]})

    # On macOS, windows can only be moved off the screen on the
    # horizontal axis.  The system menu bar also blocks windows from
    # being moved to (0,0).
    elif os == "darwin":
        assert_success(result, {"x": -8, "y": 23,
                                "width": original["width"],
                                "height": original["height"]})

    # It turns out that Windows is the only platform on which the
    # window can be reliably positioned off-screen.
    elif os == "windows_nt":
        assert_success(result, {"x": -8, "y": -8,
                                "width": original["width"],
                                "height": original["height"]})



def test_set_window_x_y_as_current(session):
    # Step 13
    get_response = session.transport.send("GET", "session/%s/window/rect" % session.session_id)
    original = get_response.body["value"]
    result = session.transport.send("POST",
                                    "session/%s/window/rect" % session.session_id,
                                    {"x": int(original["x"]),
                                     "y": int(original["y"])})
    # Step 14
    assert_success(result, {"x": original["x"],
                            "y": original["y"],
                            "width": original["width"],
                            "height": original["height"]})

def test_set_window_rect_payload(session):
    # step 14
    result = session.transport.send("POST",
                                    "session/%s/window/rect" % session.session_id,
                                    {"x": 400,
                                     "y": 400})

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
