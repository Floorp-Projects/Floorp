from tests.support.asserts import assert_error, assert_png, assert_success
from tests.support.image import png_dimensions

from . import document_dimensions


def take_full_screenshot(session):
    return session.transport.send(
        "GET",
        "/session/{session_id}/moz/screenshot/full".format(
            session_id=session.session_id
        ),
    )


def test_no_browsing_context(session, closed_window):
    response = take_full_screenshot(session)
    assert_error(response, "no such window")


def test_html_document(session, inline):
    session.url = inline("<input>")

    response = take_full_screenshot(session)
    value = assert_success(response)
    assert_png(value)
    assert png_dimensions(value) == document_dimensions(session)


def test_xhtml_document(session, inline):
    session.url = inline('<input type="text" />', doctype="xhtml")

    response = take_full_screenshot(session)
    value = assert_success(response)
    assert_png(value)
    assert png_dimensions(value) == document_dimensions(session)


def test_document_extends_beyond_viewport(session, inline):
    session.url = inline(
        """
        <style>
        body { min-height: 200vh }
        </style>
        """
    )

    response = take_full_screenshot(session)
    value = assert_success(response)
    assert_png(value)
    assert png_dimensions(value) == document_dimensions(session)
