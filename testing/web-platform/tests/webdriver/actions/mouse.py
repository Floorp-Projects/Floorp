from support.refine import get_events


def test_nothing(session, test_actions_page, mouse_chain):
    mouse_chain \
        .pointer_down(0) \
        .perform()
    assert True
