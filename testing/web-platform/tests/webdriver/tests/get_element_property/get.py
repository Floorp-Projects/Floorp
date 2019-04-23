from tests.support.asserts import assert_error, assert_success
from tests.support.inline import inline


def get_element_property(session, element_id, prop):
    return session.transport.send(
        "GET", "session/{session_id}/element/{element_id}/property/{prop}".format(
            session_id=session.session_id,
            element_id=element_id,
            prop=prop))


def test_no_browsing_context(session, closed_window):
    response = get_element_property(session, "foo", "id")
    assert_error(response, "no such window")


def test_element_not_found(session):
    response = get_element_property(session, "foo", "id")
    assert_error(response, "no such element")


def test_element_stale(session):
    session.url = inline("<input id=foobar>")
    element = session.find.css("input", all=False)
    session.refresh()

    response = get_element_property(session, element.id, "id")
    assert_error(response, "stale element reference")


def test_property_non_existent(session):
    session.url = inline("<input>")
    element = session.find.css("input", all=False)

    response = get_element_property(session, element.id, "foo")
    assert_success(response, None)
    assert session.execute_script("return arguments[0].foo", args=(element,)) is None


def test_mutated_element(session):
    session.url = inline("<input type=checkbox>")
    element = session.find.css("input", all=False)
    element.click()
    assert session.execute_script("return arguments[0].hasAttribute('checked')", args=(element,)) is False

    response = get_element_property(session, element.id, "checked")
    assert_success(response, True)
