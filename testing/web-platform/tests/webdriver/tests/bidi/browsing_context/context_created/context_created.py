import asyncio

import pytest

from webdriver.error import TimeoutException

from tests.support.sync import AsyncPoll
from . import assert_browsing_context

pytestmark = pytest.mark.asyncio

CONTEXT_CREATED_EVENT = "browsingContext.contextCreated"


async def test_not_unsubscribed(bidi_session, current_session):
    await bidi_session.session.subscribe(events=[CONTEXT_CREATED_EVENT])
    await bidi_session.session.unsubscribe(events=[CONTEXT_CREATED_EVENT])

    # Track all received browsingContext.contextCreated events in the events array
    events = []

    async def on_event(method, data):
        events.append(data)

    remove_listener = bidi_session.add_event_listener(CONTEXT_CREATED_EVENT, on_event)

    current_session.new_window(type_hint="tab")

    wait = AsyncPoll(current_session, timeout=.5)
    with pytest.raises(TimeoutException):
        await wait.until(lambda _: len(events) > 0)

    remove_listener()


@pytest.mark.parametrize("type_hint", ["tab", "window"])
async def test_new_context(bidi_session, current_session, wait_for_event, type_hint):
    # Unsubscribe in case a previous tests subscribed to the event
    await bidi_session.session.unsubscribe(events=[CONTEXT_CREATED_EVENT])

    await bidi_session.session.subscribe(events=[CONTEXT_CREATED_EVENT])

    on_entry = wait_for_event(CONTEXT_CREATED_EVENT)
    top_level_context_id = current_session.new_window(type_hint=type_hint)
    context_info = await on_entry

    assert_browsing_context(
        context_info,
        children=None,
        context=top_level_context_id,
        url="about:blank",
        parent=None,
    )


async def test_evaluate_window_open_without_url(
    bidi_session, current_session, wait_for_event
):
    # Unsubscribe in case a previous tests subscribed to the event
    await bidi_session.session.unsubscribe(events=[CONTEXT_CREATED_EVENT])

    await bidi_session.session.subscribe(events=[CONTEXT_CREATED_EVENT])

    on_entry = wait_for_event(CONTEXT_CREATED_EVENT)
    current_session.execute_script("""window.open();""")
    context_info = await on_entry

    assert_browsing_context(
        context_info,
        children=None,
        context=None,
        url="about:blank",
        parent=None,
    )

    await bidi_session.session.unsubscribe(events=[CONTEXT_CREATED_EVENT])


async def test_evaluate_window_open_with_url(
    bidi_session, current_session, wait_for_event, inline
):
    # Unsubscribe in case a previous tests subscribed to the event
    await bidi_session.session.unsubscribe(events=[CONTEXT_CREATED_EVENT])

    url = inline("<div>foo</div>")

    await bidi_session.session.subscribe(events=[CONTEXT_CREATED_EVENT])

    on_entry = wait_for_event(CONTEXT_CREATED_EVENT)
    current_session.execute_script(
        """
        const url = arguments[0];
        window.open(url);
    """,
        args=[url],
    )
    context_info = await on_entry

    assert_browsing_context(
        context_info,
        children=None,
        context=None,
        url="about:blank",
        parent=None,
    )


async def test_navigate_creates_iframes(bidi_session, current_session, wait_for_event, inline):
    # Unsubscribe in case a previous tests subscribed to the event
    await bidi_session.session.unsubscribe(events=[CONTEXT_CREATED_EVENT])

    top_level_context_id = current_session.window_handle

    url_iframe1 = inline("<div>foo</div>")
    url_iframe2 = inline("<div>bar</div>")
    url_page = inline(
        f"<iframe src='{url_iframe1}'></iframe><iframe src='{url_iframe2}'></iframe>"
    )

    events = []

    async def on_event(method, data):
        events.append(data)

    remove_listener = bidi_session.add_event_listener(CONTEXT_CREATED_EVENT, on_event)
    await bidi_session.session.subscribe(events=[CONTEXT_CREATED_EVENT])

    current_session.url = url_page

    wait = AsyncPoll(
        current_session,
        message="Didn't receive context created events for frames")
    await wait.until(lambda _: len(events) >= 2)
    assert len(events) == 2

    assert_browsing_context(
        events[0],
        children=None,
        context=None,
        url=url_iframe1,
        parent=top_level_context_id,
    )

    assert_browsing_context(
        events[1],
        children=None,
        context=None,
        url=url_iframe2,
        parent=top_level_context_id,
    )

    remove_listener()
    await bidi_session.session.unsubscribe(events=[CONTEXT_CREATED_EVENT])


async def test_navigate_creates_nested_iframes(
    bidi_session, current_session, wait_for_event, inline
):
    # Unsubscribe in case a previous tests subscribed to the event
    await bidi_session.session.unsubscribe(events=[CONTEXT_CREATED_EVENT])

    top_level_context_id = current_session.window_handle

    url_nested_iframe = inline("<div>foo</div>")
    url_iframe = inline(f"<iframe src='{url_nested_iframe}'></iframe")
    url_page = inline(f"<iframe src='{url_iframe}'></iframe>")

    events = []

    async def on_event(method, data):
        events.append(data)

    remove_listener = bidi_session.add_event_listener(CONTEXT_CREATED_EVENT, on_event)
    await bidi_session.session.subscribe(events=[CONTEXT_CREATED_EVENT])

    current_session.url = url_page

    wait = AsyncPoll(
        current_session,
        message="Didn't receive context created events for frames")
    await wait.until(lambda _: len(events) >= 2)
    assert len(events) == 2

    assert_browsing_context(
        events[0],
        children=None,
        context=None,
        url=url_iframe,
        parent=top_level_context_id,
    )

    assert_browsing_context(
        events[1],
        children=None,
        context=None,
        url=url_nested_iframe,
        parent=events[0]["context"],
    )

    remove_listener()
    await bidi_session.session.unsubscribe(events=[CONTEXT_CREATED_EVENT])
