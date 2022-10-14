import pytest

from . import using_context

pytestmark = pytest.mark.asyncio


# Helper to assert the order of top level browsing contexts.
# The window used for the assertion is inferred from the first context id of
# expected_context_ids.
def assert_tab_order(session, expected_context_ids):
    with using_context(session, "chrome"):
        context_ids = session.execute_script(
            """
            const contextId = arguments[0];
            const { TabManager } =
                ChromeUtils.importESModule("chrome://remote/content/shared/TabManager.sys.mjs");
            const browsingContext = TabManager.getBrowsingContextById(contextId);
            const chromeWindow = browsingContext.embedderElement.ownerGlobal;
            const tabBrowser = TabManager.getTabBrowser(chromeWindow);
            return tabBrowser.browsers.map(browser => TabManager.getIdForBrowser(browser));
            """,
            args=(expected_context_ids[0],),
        )

        assert context_ids == expected_context_ids


async def test_reference_context(bidi_session, current_session):
    # Create a new window with a tab tab1
    result = await bidi_session.browsing_context.create(type_hint="window")
    tab1_context_id = result["context"]

    # Create a second window with a tab tab2
    result = await bidi_session.browsing_context.create(type_hint="window")
    tab2_context_id = result["context"]

    # Create a new tab tab3 next to tab1
    result = await bidi_session.browsing_context.create(
        type_hint="tab", reference_context=tab1_context_id
    )
    tab3_context_id = result["context"]

    # Create a new tab tab4 next to tab2
    result = await bidi_session.browsing_context.create(
        type_hint="tab", reference_context=tab2_context_id
    )
    tab4_context_id = result["context"]

    # Create a new tab tab5 also next to tab2 (should consequently be between
    # tab2 and tab4)
    result = await bidi_session.browsing_context.create(
        type_hint="tab", reference_context=tab2_context_id
    )
    tab5_context_id = result["context"]

    # Create a new window, but pass a reference_context from an existing window.
    # The reference context is expected to be ignored here.
    result = await bidi_session.browsing_context.create(
        type_hint="window", reference_context=tab2_context_id
    )
    tab6_context_id = result["context"]

    # We expect 3 windows in total, with a specific tab order:
    # - the first window should contain tab1, tab3
    assert_tab_order(current_session, [tab1_context_id, tab3_context_id])
    # - the second window should contain tab2, tab5, tab4
    assert_tab_order(
        current_session, [tab2_context_id, tab5_context_id, tab4_context_id]
    )
    # - the third window should contain tab6
    assert_tab_order(current_session, [tab6_context_id])
