import pytest

from tests.support.fixtures import (
    add_browser_capabilites,
    add_event_listeners,
    configuration,
    create_dialog,
    create_frame,
    create_window,
    current_session,
    http,
    new_session,
    server_config,
    session,
    url)


pytest.fixture()(add_browser_capabilites)
pytest.fixture()(add_event_listeners)
pytest.fixture(scope="session")(configuration)
pytest.fixture()(create_dialog)
pytest.fixture()(create_frame)
pytest.fixture()(create_window)
pytest.fixture(scope="function")(current_session)
pytest.fixture()(http)
pytest.fixture(scope="function")(new_session)
pytest.fixture()(server_config)
pytest.fixture(scope="function")(session)
pytest.fixture()(url)
