import pytest

from tests.support.asserts import assert_error, assert_success, assert_dialog_handled
from tests.support.inline import inline


def read_global(session, name):
    return session.execute_script("return %s;" % name)


def get_property(session, element_id, name):
    return session.transport.send(
        "GET", "session/{session_id}/element/{element_id}/property/{name}".format(
            session_id=session.session_id, element_id=element_id, name=name))


@pytest.mark.capabilities({"unhandledPromptBehavior": "dismiss"})
def test_handle_prompt_dismiss(session, create_dialog):
    session.url = inline("<input id=foo>")
    element = session.find.css("#foo", all=False)

    create_dialog("alert", text="dismiss #1", result_var="dismiss1")

    result = get_property(session, element.id, "id")
    assert_success(result, "foo")
    assert_dialog_handled(session, "dismiss #1")

    create_dialog("confirm", text="dismiss #2", result_var="dismiss2")

    result = get_property(session, element.id, "id")
    assert_success(result, "foo")
    assert_dialog_handled(session, "dismiss #2")

    create_dialog("prompt", text="dismiss #3", result_var="dismiss3")

    result = get_property(session, element.id, "id")
    assert_success(result, "foo")
    assert_dialog_handled(session, "dismiss #3")


@pytest.mark.capabilities({"unhandledPromptBehavior": "accept"})
def test_handle_prompt_accept(session, create_dialog):
    session.url = inline("<input id=foo>")
    element = session.find.css("#foo", all=False)

    create_dialog("alert", text="dismiss #1", result_var="dismiss1")

    result = get_property(session, element.id, "id")
    assert_success(result, "foo")
    assert_dialog_handled(session, "dismiss #1")

    create_dialog("confirm", text="dismiss #2", result_var="dismiss2")

    result = get_property(session, element.id, "id")
    assert_success(result, "foo")
    assert_dialog_handled(session, "dismiss #2")

    create_dialog("prompt", text="dismiss #3", result_var="dismiss3")

    result = get_property(session, element.id, "id")
    assert_success(result, "foo")
    assert_dialog_handled(session, "dismiss #3")


def test_handle_prompt_missing_value(session, create_dialog):
    session.url = inline("<input id=foo>")
    element = session.find.css("#foo", all=False)

    create_dialog("alert", text="dismiss #1", result_var="dismiss1")

    result = get_property(session, element.id, "id")
    assert_error(result, "unexpected alert open")
    assert_dialog_handled(session, "dismiss #1")

    create_dialog("confirm", text="dismiss #2", result_var="dismiss2")

    result = get_property(session, element.id, "id")
    assert_error(result, "unexpected alert open")
    assert_dialog_handled(session, "dismiss #2")

    create_dialog("prompt", text="dismiss #3", result_var="dismiss3")

    result = get_property(session, element.id, "id")
    assert_error(result, "unexpected alert open")
    assert_dialog_handled(session, "dismiss #3")
