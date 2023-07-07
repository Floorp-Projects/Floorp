import pytest
import pytest_asyncio
from webdriver.bidi.client import BidiSession


@pytest.fixture
def match_capabilities(add_browser_capabilities):
    def match_capabilities(match_type, capability_key, capability_value):
        capability = {}
        capability[capability_key] = capability_value
        capabilities = add_browser_capabilities(capability)
        if match_type == "firstMatch":
            capabilities = [capabilities]

        capabilities_params = {}
        capabilities_params[match_type] = capabilities

        return capabilities_params

    return match_capabilities


@pytest_asyncio.fixture
async def bidi_client(browser):
    bidi_session = None
    current_browser = browser(use_bidi=True)

    async def bidi_client(capabilities={}):
        nonlocal current_browser

        # Launch the browser again if it's not running.
        if current_browser.is_running is False:
            current_browser = browser(use_bidi=True)

        server_host = current_browser.remote_agent_host
        server_port = current_browser.remote_agent_port

        nonlocal bidi_session

        bidi_session = BidiSession.bidi_only(
            f"ws://{server_host}:{server_port}", requested_capabilities=capabilities
        )
        bidi_session.current_browser = current_browser

        await bidi_session.start_transport()

        return bidi_session

    yield bidi_client

    if bidi_session is not None:
        await bidi_session.end()


@pytest_asyncio.fixture
async def new_session(bidi_client):
    """Start bidi client and create a new session.
    At the moment, it throws an error if the session was already started,
    since multiple sessions are not supported.
    """
    bidi_session = None

    async def new_session(capabilities):
        nonlocal bidi_session

        bidi_session = await bidi_client(capabilities)
        await bidi_session.start()

        return bidi_session

    yield new_session

    # Check if the browser, the session or websocket connection was not closed already.
    if (
        bidi_session is not None
        and bidi_session.current_browser.is_running is True
        and bidi_session.session_id is not None
        and bidi_session.transport.connection.closed is False
    ):
        await bidi_session.session.end()


@pytest.fixture(name="add_browser_capabilities")
def fixture_add_browser_capabilities(configuration):
    def add_browser_capabilities(capabilities):
        # Make sure there aren't keys in common.
        assert not set(configuration["capabilities"]).intersection(set(capabilities))
        result = dict(configuration["capabilities"])
        result.update(capabilities)

        return result

    return add_browser_capabilities
