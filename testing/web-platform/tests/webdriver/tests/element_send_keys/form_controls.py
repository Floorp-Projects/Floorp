import pytest

from tests.support.asserts import assert_element_has_focus, assert_events_equal
from tests.support.inline import inline

@pytest.fixture
def tracked_events():
    return [
            "blur",
            "change",
            "focus",
            "input",
            "keydown",
            "keypress",
            "keyup",
            ]

def element_send_keys(session, element, text):
    return session.transport.send(
        "POST", "/session/{session_id}/element/{element_id}/value".format(
            session_id=session.session_id,
            element_id=element.id),
        {"text": text})


def test_input(session):
    session.url = inline("<input>")
    element = session.find.css("input", all=False)
    assert element.property("value") == ""

    element_send_keys(session, element, "foo")
    assert element.property("value") == "foo"
    assert_element_has_focus(element)


def test_textarea(session):
    session.url = inline("<textarea>")
    element = session.find.css("textarea", all=False)
    assert element.property("value") == ""

    element_send_keys(session, element, "foo")
    assert element.property("value") == "foo"
    assert_element_has_focus(element)


def test_input_append(session):
    session.url = inline("<input value=a>")
    element = session.find.css("input", all=False)
    assert element.property("value") == "a"

    element_send_keys(session, element, "b")
    assert element.property("value") == "ab"

    element_send_keys(session, element, "c")
    assert element.property("value") == "abc"


def test_textarea_append(session):
    session.url = inline("<textarea>a</textarea>")
    element = session.find.css("textarea", all=False)
    assert element.property("value") == "a"

    element_send_keys(session, element, "b")
    assert element.property("value") == "ab"

    element_send_keys(session, element, "c")
    assert element.property("value") == "abc"


@pytest.mark.parametrize("tag", ["input", "textarea"])
def test_events(session, add_event_listeners, tracked_events, tag):
    expected_events = [
        "focus",
        "keydown",
        "keypress",
        "input",
        "keyup",
        "keydown",
        "keypress",
        "input",
        "keyup",
        "keydown",
        "keypress",
        "input",
        "keyup",
    ]

    session.url = inline("<%s>" % tag)
    element = session.find.css(tag, all=False)
    add_event_listeners(element, tracked_events)

    element_send_keys(session, element, "foo")
    assert element.property("value") == "foo"
    assert_events_equal(session, expected_events)


@pytest.mark.parametrize("tag", ["input", "textarea"])
def test_not_blurred(session, tag):
    session.url = inline("<%s>" % tag)
    element = session.find.css(tag, all=False)

    element_send_keys(session, element, "")
    assert_element_has_focus(element)
