import pytest


@pytest.mark.asyncio
@pytest.mark.parametrize("log_argument, expected_text", [
    ("'TEST'", "TEST"),
    ("'TWO', 'PARAMETERS'", "TWO PARAMETERS"),
    ("{}", "[object Object]"),
    ("['1', '2', '3']", "1,2,3"),
    ("null, undefined", "null undefined"),
], ids=[
    'single string',
    'two strings',
    'empty object',
    'array of strings',
    'null and undefined',
])
async def test_console_log_argument_type(bidi_session,
                                         current_session,
                                         wait_for_event,
                                         log_argument,
                                         expected_text):
    await bidi_session.session.subscribe(events=["log.entryAdded"])

    on_entry_added = wait_for_event("log.entryAdded")

    # TODO: To be replaced with the BiDi implementation of execute_script.
    current_session.execute_script(f"console.log({log_argument})")

    event_data = await on_entry_added
    assert event_data['text'] == expected_text


@pytest.mark.asyncio
@pytest.mark.parametrize("log_method, expected_level", [
    ("assert", "error"),
    ("debug", "debug"),
    ("error", "error"),
    ("info", "info"),
    ("log", "info"),
    ("table", "info"),
    ("trace", "debug"),
    ("warn", "warning"),
])
async def test_console_log_level(bidi_session,
                                 current_session,
                                 wait_for_event,
                                 log_method,
                                 expected_level):
    await bidi_session.session.subscribe(events=["log.entryAdded"])

    on_entry_added = wait_for_event("log.entryAdded")

    # TODO: To be replaced with the BiDi implementation of execute_script.
    if log_method == 'assert':
        # assert has to be called with a first falsy argument to trigger a log.
        current_session.execute_script("console.assert(false, 'text')")
    else:
        current_session.execute_script(f"console.{log_method}('text')")

    event_data = await on_entry_added

    assert event_data['text'] == 'text'
    assert event_data['level'] == expected_level
    assert event_data['type'] == 'console'
    assert event_data['method'] == log_method
    assert isinstance(event_data['timestamp'], int)


@pytest.mark.asyncio
@pytest.mark.parametrize("new_context_method_name", ["refresh", "new_window"])
async def test_console_log_new_context(bidi_session,
                                       current_session,
                                       wait_for_event,
                                       new_context_method_name):
    await bidi_session.session.subscribe(events=["log.entryAdded"])

    on_entry_added = wait_for_event("log.entryAdded")
    current_session.execute_script(f"console.log('text')")
    event_data = await on_entry_added
    assert event_data['text'] == 'text'

    new_context_method = getattr(current_session, new_context_method_name)
    new_context_method()

    on_entry_added = wait_for_event("log.entryAdded")
    current_session.execute_script(f"console.log('text_after_refresh')")
    event_data = await on_entry_added
    assert event_data['text'] == 'text_after_refresh'


@pytest.mark.asyncio
async def test_console_log_subscribe_twice(bidi_session,
                                           current_session,
                                           wait_for_event):
    # Subscribe to log.entryAdded twice and check that events are only received
    # once.
    await bidi_session.session.subscribe(events=["log.entryAdded"])
    await bidi_session.session.subscribe(events=["log.entryAdded"])

    # Track all received log.entryAdded events in the events array
    events = []
    async def on_event(method, data):
        events.append(data)

    remove_listener = bidi_session.add_event_listener("log.entryAdded", on_event)

    on_entry_added = wait_for_event("log.entryAdded")
    current_session.execute_script(f"console.log('text1')")
    await on_entry_added
    assert len(events) == 1;
    assert events[0]['text'] == 'text1'

    # Wait for another console log so that potential duplicates for the first
    # log have time to be received.
    on_entry_added = wait_for_event("log.entryAdded")
    current_session.execute_script(f"console.log('text2')")
    await on_entry_added
    assert len(events) == 2;
    assert events[1]['text'] == 'text2'

    remove_listener()
