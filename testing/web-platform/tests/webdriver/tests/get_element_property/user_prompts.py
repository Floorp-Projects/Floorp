import pytest

from tests.support.asserts import assert_error, assert_success, assert_dialog_handled
from tests.support.inline import inline


def get_property(session, element_id, name):
    return session.transport.send(
        "GET", "session/{session_id}/element/{element_id}/property/{name}".format(
            session_id=session.session_id, element_id=element_id, name=name))


@pytest.mark.capabilities({"unhandledPromptBehavior": "dismiss"})
@pytest.mark.parametrize("dialog_type", ["alert", "confirm", "prompt"])
def test_handle_prompt_dismiss(session, create_dialog, dialog_type):
    session.url = inline("<input id=foo>")
    element = session.find.css("#foo", all=False)

    create_dialog(dialog_type, text="dialog")

    response = get_property(session, element.id, "id")
    assert_success(response, "foo")

    assert_dialog_handled(session, expected_text="dialog")


@pytest.mark.capabilities({"unhandledPromptBehavior": "accept"})
@pytest.mark.parametrize("dialog_type", ["alert", "confirm", "prompt"])
def test_handle_prompt_accept(session, create_dialog, dialog_type):
    session.url = inline("<input id=foo>")
    element = session.find.css("#foo", all=False)

    create_dialog(dialog_type, text="dialog")

    response = get_property(session, element.id, "id")
    assert_success(response, "foo")

    assert_dialog_handled(session, expected_text="dialog")


@pytest.mark.parametrize("dialog_type", ["alert", "confirm", "prompt"])
def test_handle_prompt_missing_value(session, create_dialog, dialog_type):
    session.url = inline("<input id=foo>")
    element = session.find.css("#foo", all=False)

    create_dialog(dialog_type, text="dialog")

    response = get_property(session, element.id, "id")
    assert_error(response, "unexpected alert open")

    assert_dialog_handled(session, expected_text="dialog")
