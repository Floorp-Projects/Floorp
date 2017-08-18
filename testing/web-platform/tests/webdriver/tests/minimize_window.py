from tests.support.inline import inline
from tests.support.asserts import assert_error, assert_success


alert_doc = inline("<script>window.alert()</script>")


def minimize(session):
    return session.transport.send("POST", "session/%s/window/minimize" % session.session_id)


# 10.7.4 Minimize Window


def test_minimize_no_browsing_context(session, create_window):
    # step 1
    session.window_handle = create_window()
    session.close()
    response = minimize(session)
    assert_error(response, "no such window")


def test_handle_user_prompt(session):
    # step 2
    session.url = alert_doc
    response = minimize(session)
    assert_error(response, "unexpected alert open")


def test_minimize(session):
    assert session.window.state == "normal"

    # step 4
    response = minimize(session)
    assert_success(response)

    assert session.window.state == "minimized"


def test_payload(session):
    assert session.window.state == "normal"

    response = minimize(session)

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

    assert session.window.state == "minimized"


def test_minimize_twice_is_idempotent(session):
    assert session.execute_script("return document.hidden") is False

    first_response = minimize(session)
    assert_success(first_response)
    assert session.execute_script("return document.hidden") is True

    second_response = minimize(session)
    assert_success(second_response)
    assert session.execute_script("return document.hidden") is True
