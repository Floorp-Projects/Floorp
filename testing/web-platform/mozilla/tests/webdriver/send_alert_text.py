from tests.support.asserts import assert_error
from tests.support.http_handlers.authentication import basic_authentication


def send_alert_text(session, text=None):
    return session.transport.send(
        "POST",
        "session/{session_id}/alert/text".format(**vars(session)),
        {"text": text},
    )


def test_basic_auth_unsupported_operation(url, session):
    """
    Basic auth dialogues are not included in HTML's definition of
    'user prompts': those are limited to the 'simple dialogues'
    such as window.alert(), window.prompt() et al. and the print
    dialogue.
    """
    session.url = basic_authentication(url)
    response = send_alert_text(session, "Federer")
    assert_error(response, "unsupported operation")
