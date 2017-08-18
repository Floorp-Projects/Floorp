# META: timeout=long

from tests.support.inline import inline
from tests.support.asserts import assert_error, assert_success, assert_dialog_handled
from tests.support.fixtures import create_dialog


alert_doc = inline("<script>window.alert()</script>")


def read_global(session, name):
    return session.execute_script("return %s;" % name)


def fullscreen(session):
    return session.transport.send("POST", "session/%s/window/fullscreen" % session.session_id)


# 10.7.5 Fullscreen Window


# 1. If the current top-level browsing context is no longer open, return error
#    with error code no such window.
def test_no_browsing_context(session, create_window):
    # step 1
    session.window_handle = create_window()
    session.close()
    response = fullscreen(session)
    assert_error(response, "no such window")


# [...]
# 2. Handle any user prompts and return its value if it is an error.
# [...]
# In order to handle any user prompts a remote end must take the following
# steps:
# 2. Run the substeps of the first matching user prompt handler:
#
#    [...]
#    - accept state
#      1. Accept the current user prompt.
#    [...]
#
# 3. Return success.
def test_handle_prompt_accept(new_session):
    _, session = new_session({"alwaysMatch": {"unhandledPromptBehavior": "accept"}})
    session.url = inline("<title>WD doc title</title>")
    create_dialog(session)("alert", text="accept #1", result_var="accept1")

    expected_title = read_global(session, "document.title")
    response = fullscreen(session)

    assert_success(response, expected_title)
    assert_dialog_handled(session, "accept #1")
    assert read_global(session, "accept1") == None

    expected_title = read_global(session, "document.title")
    create_dialog(session)("confirm", text="accept #2", result_var="accept2")

    response = fullscreen(session)

    assert_success(response, expected_title)
    assert_dialog_handled(session, "accept #2")
    assert read_global(session, "accept2"), True

    expected_title = read_global(session, "document.title")
    create_dialog(session)("prompt", text="accept #3", result_var="accept3")

    response = fullscreen(session)

    assert_success(response, expected_title)
    assert_dialog_handled(session, "accept #3")
    assert read_global(session, "accept3") == ""


# [...]
# 2. Handle any user prompts and return its value if it is an error.
# [...]
# In order to handle any user prompts a remote end must take the following
# steps:
# 2. Run the substeps of the first matching user prompt handler:
#
#    [...]
#    - missing value default state
#    - not in the table of simple dialogs
#      1. Dismiss the current user prompt.
#      2. Return error with error code unexpected alert open.
def test_handle_prompt_missing_value(session, create_dialog):
    session.url = inline("<title>WD doc title</title>")
    create_dialog("alert", text="dismiss #1", result_var="dismiss1")

    response = fullscreen(session)

    assert_error(response, "unexpected alert open")
    assert_dialog_handled(session, "dismiss #1")
    assert read_global(session, "accept1") == None

    create_dialog("confirm", text="dismiss #2", result_var="dismiss2")

    response = fullscreen(session)

    assert_error(response, "unexpected alert open")
    assert_dialog_handled(session, "dismiss #2")
    assert read_global(session, "dismiss2") == False

    create_dialog("prompt", text="dismiss #3", result_var="dismiss3")

    response = fullscreen(session)

    assert_error(response, "unexpected alert open")
    assert_dialog_handled(session, "dismiss #3")
    assert read_global(session, "dismiss3") == None


# 4. Call fullscreen an element with the current top-level browsing
# context's active document's document element.
def test_fullscreen(session):
    # step 4
    response = fullscreen(session)
    assert_success(response)
    assert session.execute_script("return window.fullScreen") == True


# 5. Return success with the JSON serialization of the current top-level
# browsing context's window rect.
#
# [...]
#
# A top-level browsing context's window rect is defined as a
# dictionary of the screenX, screenY, width and height attributes of the
# WindowProxy. Its JSON representation is the following:
#
#   x
#     WindowProxy's screenX attribute.
#
#   y
#     WindowProxy's screenY attribute.
#
#   width
#     Width of the top-level browsing context's outer dimensions,
#     including any browser chrome and externally drawn window
#     decorations in CSS reference pixels.
#
#   height
#     Height of the top-level browsing context's outer dimensions,
#     including any browser chrome and externally drawn window decorations
#     in CSS reference pixels.
#
#   state
#     The top-level browsing context's window state.
#
# [...]
#
# The top-level browsing context has an associated window state which
# describes what visibility state its OS widget window is in. It can be
# in one of the following states:
#
#   "maximized"
#     The window is maximized.
#
#   "minimized"
#     The window is iconified.
#
#   "normal"
#     The window is shown normally.
#
#   "fullscreen"
#     The window is in full screen mode.
#
# If for whatever reason the top-level browsing context's OS window
# cannot enter either of the window states, or if this concept is not
# applicable on the current system, the default state must be normal.
def test_payload(session):
    response = fullscreen(session)

    # step 5
    assert response.status == 200
    assert isinstance(response.body["value"], dict)

    rect = response.body["value"]
    assert "width" in rect
    assert "height" in rect
    assert "x" in rect
    assert "y" in rect
    assert isinstance(rect["width"], (int, float))
    assert isinstance(rect["height"], (int, float))
    assert isinstance(rect["x"], (int, float))
    assert isinstance(rect["y"], (int, float))


def test_fullscreen_twice_is_idempotent(session):
    assert session.execute_script("return window.fullScreen") is False

    first_response = fullscreen(session)
    assert_success(first_response)
    assert session.execute_script("return window.fullScreen") is True

    second_response = fullscreen(session)
    assert_success(second_response)
    assert session.execute_script("return window.fullScreen") is True
