import math
import time

import pytest

from webdriver.bidi.modules.script import ContextTarget

from . import assert_console_entry


@pytest.mark.asyncio
@pytest.mark.parametrize(
    "log_argument, expected_text",
    [
        ("'TEST'", "TEST"),
        ("'TWO', 'PARAMETERS'", "TWO PARAMETERS"),
        ("{}", "[object Object]"),
        ("['1', '2', '3']", "1,2,3"),
        ("null, undefined", "null undefined"),
    ],
    ids=[
        "single string",
        "two strings",
        "empty object",
        "array of strings",
        "null and undefined",
    ],
)
async def test_text_with_argument_variation(
    bidi_session,
    current_session,
    wait_for_event,
    log_argument,
    expected_text,
    top_context,
):
    await bidi_session.session.subscribe(events=["log.entryAdded"])

    on_entry_added = wait_for_event("log.entryAdded")

    # TODO: To be replaced with the BiDi implementation of execute_script.
    current_session.execute_script(f"console.log({log_argument})")

    event_data = await on_entry_added

    assert_console_entry(event_data, text=expected_text, context=top_context["context"])


@pytest.mark.asyncio
@pytest.mark.parametrize(
    "log_method, expected_level",
    [
        ("assert", "error"),
        ("debug", "debug"),
        ("error", "error"),
        ("info", "info"),
        ("log", "info"),
        ("table", "info"),
        ("trace", "debug"),
        ("warn", "warning"),
    ],
)
async def test_level(
    bidi_session, current_session, wait_for_event, log_method, expected_level
):
    await bidi_session.session.subscribe(events=["log.entryAdded"])

    on_entry_added = wait_for_event("log.entryAdded")

    # TODO: To be replaced with the BiDi implementation of execute_script.
    if log_method == "assert":
        # assert has to be called with a first falsy argument to trigger a log.
        current_session.execute_script("console.assert(false, 'foo')")
    else:
        current_session.execute_script(f"console.{log_method}('foo')")

    event_data = await on_entry_added

    assert_console_entry(
        event_data, text="foo", level=expected_level, method=log_method
    )


@pytest.mark.asyncio
async def test_timestamp(bidi_session, current_session, current_time, wait_for_event):
    await bidi_session.session.subscribe(events=["log.entryAdded"])

    on_entry_added = wait_for_event("log.entryAdded")

    time_start = current_time()

    # TODO: To be replaced with the BiDi implementation of execute_async_script.
    current_session.execute_async_script(
        """
        const resolve = arguments[0];
        setTimeout(() => {
            console.log('foo');
            resolve();
        }, 100);
        """
    )

    event_data = await on_entry_added

    time_end = current_time()

    assert_console_entry(
        event_data, text="foo", time_start=time_start, time_end=time_end
    )


@pytest.mark.asyncio
async def test_new_context_with_new_window(
    bidi_session, current_session, wait_for_event, top_context
):
    await bidi_session.session.subscribe(events=["log.entryAdded"])

    on_entry_added = wait_for_event("log.entryAdded")
    current_session.execute_script("console.log('foo')")
    event_data = await on_entry_added
    assert_console_entry(event_data, text="foo", context=top_context["context"])

    new_window_handle = current_session.new_window()
    current_session.window_handle = new_window_handle

    on_entry_added = wait_for_event("log.entryAdded")
    current_session.execute_script("console.log('foo_in_new_window')")
    event_data = await on_entry_added
    assert_console_entry(
        event_data, text="foo_in_new_window", context=new_window_handle
    )


@pytest.mark.asyncio
async def test_new_context_with_refresh(
    bidi_session, current_session, wait_for_event, top_context
):
    await bidi_session.session.subscribe(events=["log.entryAdded"])

    on_entry_added = wait_for_event("log.entryAdded")
    current_session.execute_script("console.log('foo')")
    event_data = await on_entry_added
    assert_console_entry(event_data, text="foo", context=top_context["context"])

    current_session.refresh()

    on_entry_added = wait_for_event("log.entryAdded")
    current_session.execute_script("console.log('foo_after_refresh')")
    event_data = await on_entry_added
    assert_console_entry(
        event_data, text="foo_after_refresh", context=top_context["context"]
    )


@pytest.mark.asyncio
async def test_different_contexts(
    bidi_session,
    wait_for_event,
    test_page_same_origin_frame,
    top_context,
):
    await bidi_session.browsing_context.navigate(
        context=top_context["context"], url=test_page_same_origin_frame, wait="complete"
    )
    contexts = await bidi_session.browsing_context.get_tree(root=top_context["context"])
    assert len(contexts[0]["children"]) == 1
    frame_context = contexts[0]["children"][0]

    await bidi_session.session.subscribe(events=["log.entryAdded"])

    on_entry_added = wait_for_event("log.entryAdded")
    await bidi_session.script.evaluate(
        expression="console.log('foo')",
        target=ContextTarget(top_context["context"]),
        await_promise=True,
    )
    event_data = await on_entry_added
    assert_console_entry(event_data, text="foo", context=top_context["context"])

    on_entry_added = wait_for_event("log.entryAdded")
    await bidi_session.script.evaluate(
        expression="console.log('bar')",
        target=ContextTarget(frame_context["context"]),
        await_promise=True,
    )
    event_data = await on_entry_added
    assert_console_entry(event_data, text="bar", context=frame_context["context"])
