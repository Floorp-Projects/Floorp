import pytest
from tests.support.asserts import assert_success
from tests.support.keys import Keys


def element_send_keys(session, element, text):
    return session.transport.send(
        "POST",
        "/session/{session_id}/element/{element_id}/value".format(
            session_id=session.session_id, element_id=element.id
        ),
        {"text": text},
    )


def test_modifier_key_toggles(session, inline, modifier_key):
    session.url = inline("<input type=text value=foo>")
    element = session.find.css("input", all=False)

    response = element_send_keys(
        session, element, f"{modifier_key}a{modifier_key}{Keys.DELETE}cheese"
    )
    assert_success(response)

    assert element.property("value") == "cheese"


@pytest.mark.parametrize("dispatch_once_per_surrogate_pair", [False, True])
def test_dispatch_once_per_surrogate_pair(
    session, use_pref, inline, dispatch_once_per_surrogate_pair
):
    use_pref(
        "dom.event.keypress.dispatch_once_per_surrogate_pair",
        dispatch_once_per_surrogate_pair,
    )

    session.url = inline("<input>")
    element = session.find.css("input", all=False)

    text = "🦥🍄"
    response = element_send_keys(session, element, text)
    assert_success(response)

    assert element.property("value") == text
