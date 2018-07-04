import pytest

from tests.support.asserts import assert_error, assert_dialog_handled
from tests.support.inline import inline


def element_send_keys(session, element, text):
    return session.transport.send(
        "POST", "/session/{session_id}/element/{element_id}/value".format(
            session_id=session.session_id,
            element_id=element.id),
        {"text": text})


def test_handle_prompt_dismiss_and_notify():
    """TODO"""


def test_handle_prompt_accept_and_notify():
    """TODO"""


def test_handle_prompt_ignore():
    """TODO"""


@pytest.mark.capabilities({"unhandledPromptBehavior": "accept"})
def test_handle_prompt_accept(session, create_dialog):
    session.url = inline("<input>")
    element = session.find.css("input", all=False)

    create_dialog("alert", text="dismiss #1", result_var="dismiss1")
    response = element_send_keys(session, element, "foo")
    assert response.status == 200
    assert_dialog_handled(session, "dismiss #1")

    create_dialog("confirm", text="dismiss #2", result_var="dismiss2")
    response = element_send_keys(session, element, "foo")
    assert response.status == 200
    assert_dialog_handled(session, "dismiss #2")

    create_dialog("prompt", text="dismiss #3", result_var="dismiss3")
    response = element_send_keys(session, element, "foo")
    assert response.status == 200
    assert_dialog_handled(session, "dismiss #3")


def test_handle_prompt_missing_value(session, create_dialog):
    session.url = inline("<input>")
    element = session.find.css("input", all=False)

    create_dialog("alert", text="dismiss #1", result_var="dismiss1")

    response = element_send_keys(session, element, "foo")
    assert_error(response, "unexpected alert open")
    assert_dialog_handled(session, "dismiss #1")

    create_dialog("confirm", text="dismiss #2", result_var="dismiss2")
    response = element_send_keys(session, element, "foo")
    assert_error(response, "unexpected alert open")
    assert_dialog_handled(session, "dismiss #2")

    create_dialog("prompt", text="dismiss #3", result_var="dismiss3")
    response = element_send_keys(session, element, "foo")
    assert_error(response, "unexpected alert open")
    assert_dialog_handled(session, "dismiss #3")
