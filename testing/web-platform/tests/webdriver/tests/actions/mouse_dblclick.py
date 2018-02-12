import pytest

from tests.actions.support.mouse import get_inview_center, get_viewport_rect
from tests.actions.support.refine import get_events, filter_dict
from tests.support.asserts import assert_move_to_coordinates

_DBLCLICK_INTERVAL = 640


@pytest.mark.parametrize("click_pause", [0, 200])
def test_dblclick_at_coordinates(session, test_actions_page, mouse_chain, click_pause):
    div_point = {
        "x": 82,
        "y": 187,
    }
    mouse_chain \
        .pointer_move(div_point["x"], div_point["y"]) \
        .click() \
        .pause(click_pause) \
        .click() \
        .perform()
    events = get_events(session)
    assert_move_to_coordinates(div_point, "outer", events)
    expected = [
        {"type": "mousedown", "button": 0},
        {"type": "mouseup", "button": 0},
        {"type": "click", "button": 0},
        {"type": "mousedown", "button": 0},
        {"type": "mouseup", "button": 0},
        {"type": "click", "button": 0},
        {"type": "dblclick", "button": 0},
    ]
    assert len(events) == 8
    filtered_events = [filter_dict(e, expected[0]) for e in events]
    assert expected == filtered_events[1:]


def test_dblclick_with_pause_after_second_pointerdown(session, test_actions_page, mouse_chain):
        outer = session.find.css("#outer", all=False)
        center = get_inview_center(outer.rect, get_viewport_rect(session))
        mouse_chain \
            .pointer_move(int(center["x"]), int(center["y"])) \
            .click() \
            .pointer_down() \
            .pause(_DBLCLICK_INTERVAL + 10) \
            .pointer_up() \
            .perform()
        events = get_events(session)
        expected = [
            {"type": "mousedown", "button": 0},
            {"type": "mouseup", "button": 0},
            {"type": "click", "button": 0},
            {"type": "mousedown", "button": 0},
            {"type": "mouseup", "button": 0},
            {"type": "click", "button": 0},
            {"type": "dblclick", "button": 0},
        ]
        assert len(events) == 8
        filtered_events = [filter_dict(e, expected[0]) for e in events]
        assert expected == filtered_events[1:]


def test_no_dblclick(session, test_actions_page, mouse_chain):
        outer = session.find.css("#outer", all=False)
        center = get_inview_center(outer.rect, get_viewport_rect(session))
        mouse_chain \
            .pointer_move(int(center["x"]), int(center["y"])) \
            .click() \
            .pause(_DBLCLICK_INTERVAL + 10) \
            .click() \
            .perform()
        events = get_events(session)
        expected = [
            {"type": "mousedown", "button": 0},
            {"type": "mouseup", "button": 0},
            {"type": "click", "button": 0},
            {"type": "mousedown", "button": 0},
            {"type": "mouseup", "button": 0},
            {"type": "click", "button": 0},
        ]
        assert len(events) == 7
        filtered_events = [filter_dict(e, expected[0]) for e in events]
        assert expected == filtered_events[1:]
