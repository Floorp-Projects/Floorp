import pytest

from webdriver.bidi.client import BidiSession

pytestmark = pytest.mark.asyncio


async def test_navigator_webdriver_enabled(inline, browser):
    # Request a new browser with only WebDriver BiDi and not Marionette/CDP enabled.
    current_browser = browser(use_bidi=True, extra_prefs={"remote.active-protocols": 1})
    server_port = current_browser.remote_agent_port

    async with BidiSession.bidi_only(f"ws://localhost:{server_port}") as bidi_session:
        # Until script.evaluate has been implemented use console logging
        # as workaround to retrieve the value for navigator.webdriver.
        url = inline(
            """
            <script>console.log("navigator.webdriver", navigator.webdriver);</script>
            """
        )

        await bidi_session.session.subscribe(events=["log.entryAdded"])

        on_entry_added = bidi_session.event_loop.create_future()

        async def on_event(method, data):
            remove_listener()
            on_entry_added.set_result(data)

        remove_listener = bidi_session.add_event_listener("log.entryAdded", on_event)

        contexts = await bidi_session.browsing_context.get_tree(max_depth=0)
        assert len(contexts) > 0

        await bidi_session.browsing_context.navigate(
            context=contexts[0]["context"], url=url, wait="complete"
        )

        event_data = await on_entry_added
        assert event_data["args"][0]["value"] == "navigator.webdriver"
        assert event_data["args"][1]["value"] is True
