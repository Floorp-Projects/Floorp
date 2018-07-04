# META: timeout=long

import pytest

from webdriver import error

from tests.support.asserts import assert_success


def execute_async_script(session, script, args=None):
    if args is None:
        args = []
    body = {"script": script, "args": args}

    return session.transport.send(
        "POST", "/session/{session_id}/execute/async".format(**vars(session)),
        body)


@pytest.mark.capabilities({"unhandledPromptBehavior": "accept"})
def test_handle_prompt_accept(session):
    response = execute_async_script(session, "window.alert('Hello');")
    assert_success(response, None)

    session.title
    with pytest.raises(error.NoSuchAlertException):
        session.alert.accept()


@pytest.mark.capabilities({"unhandledPromptBehavior": "dismiss"})
def test_handle_prompt_dismiss(session):
    response = execute_async_script(session, "window.alert('Hello');")
    assert_success(response, None)

    session.title
    with pytest.raises(error.NoSuchAlertException):
        session.alert.dismiss()


@pytest.mark.capabilities({"unhandledPromptBehavior": "dismiss and notify"})
def test_handle_prompt_dismiss_and_notify(session):
    response = execute_async_script(session, "window.alert('Hello');")
    assert_success(response, None)

    with pytest.raises(error.UnexpectedAlertOpenException):
        session.title
    with pytest.raises(error.NoSuchAlertException):
        session.alert.dismiss()


@pytest.mark.capabilities({"unhandledPromptBehavior": "accept and notify"})
def test_handle_prompt_accept_and_notify(session):
    response = execute_async_script(session, "window.alert('Hello');")
    assert_success(response, None)

    with pytest.raises(error.UnexpectedAlertOpenException):
        session.title
    with pytest.raises(error.NoSuchAlertException):
        session.alert.accept()


@pytest.mark.capabilities({"unhandledPromptBehavior": "ignore"})
def test_handle_prompt_ignore(session):
    response = execute_async_script(session, "window.alert('Hello');")
    assert_success(response, None)

    with pytest.raises(error.UnexpectedAlertOpenException):
        session.title
    session.alert.dismiss()


def test_handle_prompt_default(session):
    response = execute_async_script(session, "window.alert('Hello');")
    assert_success(response, None)

    with pytest.raises(error.UnexpectedAlertOpenException):
        session.title
    with pytest.raises(error.NoSuchAlertException):
        session.alert.dismiss()


@pytest.mark.capabilities({"unhandledPromptBehavior": "accept"})
def test_handle_prompt_twice(session):
    response = execute_async_script(session, "window.alert('Hello');window.alert('Bye');")
    assert_success(response, None)

    session.alert.dismiss()
    # The first alert has been accepted by the user prompt handler, the second one remains.
    # FIXME: this is how browsers currently work, but the spec should clarify if this is the
    #        expected behavior, see https://github.com/w3c/webdriver/issues/1153.
    assert session.alert.text == "Bye"
    session.alert.dismiss()
