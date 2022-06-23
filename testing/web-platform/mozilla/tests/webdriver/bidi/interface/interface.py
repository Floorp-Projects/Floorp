import pytest

from webdriver.bidi.client import BidiSession
from webdriver.bidi.modules.script import ContextTarget

pytestmark = pytest.mark.asyncio


async def test_navigator_webdriver_enabled(inline, browser):
    # Request a new browser with only WebDriver BiDi and not Marionette/CDP enabled.
    current_browser = browser(use_bidi=True, extra_prefs={"remote.active-protocols": 1})
    server_port = current_browser.remote_agent_port

    async with BidiSession.bidi_only(f"ws://localhost:{server_port}") as bidi_session:
        contexts = await bidi_session.browsing_context.get_tree(max_depth=0)
        assert len(contexts) > 0

        result = await bidi_session.script.evaluate(
            expression="navigator.webdriver",
            target=ContextTarget(contexts[0]["context"]),
            await_promise=False,
        )

        assert result == {"type": "boolean", "value": True}
