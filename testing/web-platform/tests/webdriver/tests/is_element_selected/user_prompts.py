import pytest

from tests.support.asserts import assert_error, assert_dialog_handled, assert_success
from tests.support.inline import inline
from tests.support.fixtures import create_dialog


def is_element_selected(session, element_id):
    return session.transport.send(
        "GET", "session/{session_id}/element/{element_id}/selected".format(
            session_id=session.session_id,
            element_id=element_id))


@pytest.mark.capabilities({"unhandledPromptBehavior": "dismiss"})
def test_handle_prompt_dismiss(session):
    session.url = inline("<input id=foo>")
    element = session.find.css("#foo", all=False)

    create_dialog(session)("alert", text="dismiss #1", result_var="dismiss1")

    result = is_element_selected(session, element.id)
    assert_success(result, False)
    assert_dialog_handled(session, "dismiss #1")

    create_dialog(session)("confirm", text="dismiss #2", result_var="dismiss2")

    result = is_element_selected(session, element.id)
    assert_success(result, False)
    assert_dialog_handled(session, "dismiss #2")

    create_dialog(session)("prompt", text="dismiss #3", result_var="dismiss3")

    result = is_element_selected(session, element.id)
    assert_success(result, False)
    assert_dialog_handled(session, "dismiss #3")


@pytest.mark.capabilities({"unhandledPromptBehavior": "accept"})
def test_handle_prompt_accept(session):
    session.url = inline("<input id=foo>")
    element = session.find.css("#foo", all=False)

    create_dialog(session)("alert", text="dismiss #1", result_var="dismiss1")

    result = is_element_selected(session, element.id)
    assert_success(result, False)
    assert_dialog_handled(session, "dismiss #1")

    create_dialog(session)("confirm", text="dismiss #2", result_var="dismiss2")

    result = is_element_selected(session, element.id)
    assert_success(result, False)
    assert_dialog_handled(session, "dismiss #2")

    create_dialog(session)("prompt", text="dismiss #3", result_var="dismiss3")

    result = is_element_selected(session, element.id)
    assert_success(result, False)
    assert_dialog_handled(session, "dismiss #3")


def test_handle_prompt_missing_value(session):
    # 13.1 step 2
    session.url = inline("<input id=foo>")
    element = session.find.css("#foo", all=False)

    create_dialog(session)("alert", text="dismiss #1", result_var="dismiss1")

    result = is_element_selected(session, element.id)
    assert_error(result, "unexpected alert open")
    assert_dialog_handled(session, "dismiss #1")

    create_dialog(session)("confirm", text="dismiss #2", result_var="dismiss2")

    result = is_element_selected(session, element.id)
    assert_error(result, "unexpected alert open")
    assert_dialog_handled(session, "dismiss #2")

    create_dialog(session)("prompt", text="dismiss #3", result_var="dismiss3")

    result = is_element_selected(session, element.id)
    assert_error(result, "unexpected alert open")
    assert_dialog_handled(session, "dismiss #3")
