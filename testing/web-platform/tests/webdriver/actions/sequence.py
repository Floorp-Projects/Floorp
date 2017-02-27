from support.refine import get_keys, filter_dict


def get_events(session):
    """Return list of key events recorded in the test_keys_page fixture."""
    events = session.execute_script("return allEvents.events;") or []
    # `key` values in `allEvents` may be escaped (see `escapeSurrogateHalf` in
    # test_keys_wdspec.html), so this converts them back into unicode literals.
    for e in events:
        # example: turn "U+d83d" (6 chars) into u"\ud83d" (1 char)
        if e["key"].startswith(u"U+"):
            key = e["key"]
            hex_suffix = key[key.index("+") + 1:]
            e["key"] = unichr(int(hex_suffix, 16))
    return events


def test_no_actions_send_no_events(session, key_reporter, key_chain):
    key_chain.perform()
    assert len(get_keys(key_reporter)) == 0
    assert len(get_events(session)) == 0


def test_release_char_sequence_sends_keyup_events_in_reverse(session,
                                                             key_reporter,
                                                             key_chain):
    key_chain \
        .key_down("a") \
        .key_down("b") \
        .perform()
    # reset so we only see the release events
    session.execute_script("resetEvents();")
    session.actions.release()
    expected = [
        {"code": "KeyB", "key": "b", "type": "keyup"},
        {"code": "KeyA", "key": "a", "type": "keyup"},
    ]
    events = [filter_dict(e, expected[0]) for e in get_events(session)]
    assert events == expected


def test_release_no_actions_sends_no_events(session, key_reporter):
    session.actions.release()
    assert len(get_keys(key_reporter)) == 0
    assert len(get_events(session)) == 0
