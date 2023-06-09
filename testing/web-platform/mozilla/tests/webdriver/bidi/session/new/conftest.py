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
async def new_session(browser):
    current_browser = browser(use_bidi=True)
    bidi_session = None
    keep_browser = False

    server_host = current_browser.remote_agent_host
    server_port = current_browser.remote_agent_port

    async def new_session(capabilities, keep_browser_open=False):
        nonlocal bidi_session
        nonlocal keep_browser

        keep_browser = keep_browser_open

        bidi_session = BidiSession.bidi_only(
            f"ws://{server_host}:{server_port}", requested_capabilities=capabilities
        )
        await bidi_session.start()

        return bidi_session

    yield new_session

    await bidi_session.end()

    if keep_browser is False and current_browser is not None:
        current_browser.quit()
        current_browser = None


@pytest.fixture(name="add_browser_capabilities")
def fixture_add_browser_capabilities(configuration):
    def add_browser_capabilities(capabilities):
        # Make sure there aren't keys in common.
        assert not set(configuration["capabilities"]).intersection(set(capabilities))
        result = dict(configuration["capabilities"])
        result.update(capabilities)

        return result

    return add_browser_capabilities
