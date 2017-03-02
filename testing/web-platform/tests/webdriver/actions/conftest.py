import pytest


@pytest.fixture
def key_chain(session):
    return session.actions.sequence("key", "keyboard_id")


@pytest.fixture(autouse=True)
def release_actions(session, request):
    # release all actions after each test
    # equivalent to a teardown_function, but with access to session fixture
    request.addfinalizer(session.actions.release)


@pytest.fixture
def key_reporter(session, test_actions_page, request):
    """Represents focused input element from `test_keys_page` fixture."""
    input_el = session.find.css("#keys", all=False)
    input_el.click()
    return input_el


@pytest.fixture
def test_actions_page(session, server):
    session.url = server.where_is("webdriver/actions/support/test_actions_wdspec.html")
