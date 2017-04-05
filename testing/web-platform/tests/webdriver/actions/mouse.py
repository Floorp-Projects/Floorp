import urllib

from support.refine import get_events, filter_dict


# TODO use support.inline module once available from upstream
def inline(doc):
    return "data:text/html;charset=utf-8,%s" % urllib.quote(doc)


def link_doc(dest):
    content = "<a href=\"{}\" id=\"link\">destination</a>".format(dest)
    return inline(content)


def test_click_at_coordinates(session, test_actions_page, mouse_chain):
    div_point = {
        "x": 82,
        "y": 187,
    }
    mouse_chain \
        .pointer_move(div_point["x"], div_point["y"], duration=1000) \
        .click() \
        .perform()
    events = get_events(session)
    assert len(events) == 4
    for e in events:
        if e["type"] != "mousemove":
            assert e["pageX"] == div_point["x"]
            assert e["pageY"] == div_point["y"]
            assert e["target"] == "outer"
        if e["type"] != "mousedown":
            assert e["buttons"] == 0
        assert e["button"] == 0
    expected = [
        {"type": "mousedown", "buttons": 1},
        {"type": "mouseup",  "buttons": 0},
        {"type": "click", "buttons": 0},
    ]
    filtered_events = [filter_dict(e, expected[0]) for e in events]
    assert expected == filtered_events[1:]


def test_click_element_center(session, test_actions_page, mouse_chain):
    outer = session.find.css("#outer", all=False)
    outer_rect = outer.rect
    center_x = outer_rect["width"] / 2 + outer_rect["x"]
    center_y = outer_rect["height"] / 2 + outer_rect["y"]
    mouse_chain.click(element=outer).perform()
    events = get_events(session)
    assert len(events) == 4
    event_types = [e["type"] for e in events]
    assert ["mousemove", "mousedown", "mouseup", "click"] == event_types
    for e in events:
        if e["type"] != "mousemove":
            # TODO use pytest.approx once we upgrade to pytest > 3.0
            assert abs(e["pageX"] - center_x) < 1
            assert abs(e["pageY"] - center_y) < 1
            assert e["target"] == "outer"


def test_click_navigation(session, url):
    destination = url("/webdriver/actions/support/test_actions_wdspec.html")
    start = link_doc(destination)

    def click(link):
        mouse_chain = session.actions.sequence(
            "pointer", "pointer_id", {"pointerType": "mouse"})
        mouse_chain.click(element=link).pause(300).perform()

    session.url = start
    click(session.find.css("#link", all=False))
    assert session.url == destination
    # repeat steps to check behaviour after document unload
    session.url = start
    click(session.find.css("#link", all=False))
    assert session.url == destination
