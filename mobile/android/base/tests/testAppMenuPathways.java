package org.mozilla.gecko.tests;

import org.json.JSONObject;
import org.mozilla.gecko.Tabs;
import org.mozilla.gecko.tests.components.AppMenuComponent;
import org.mozilla.gecko.tests.helpers.GeckoHelper;
import org.mozilla.gecko.tests.helpers.NavigationHelper;

/**
 * Set of tests to test UI App menu and submenus the user interact with.
 */
public class testAppMenuPathways extends UITest {

    /**
     * Robocop supports only a single test function per test class. Therefore, we
     * have a single top-level test function that dispatches to sub-tests.
     */
    public void testAppMenuPathways() {
        GeckoHelper.blockForReady();

        _testSaveAsPDFPathway();
    }

    public void _testSaveAsPDFPathway() {
        // Page menu should be disabled in about:home.
        mAppMenu.assertMenuItemIsDisabledAndVisible(AppMenuComponent.PageMenuItem.SAVE_AS_PDF);

        // Generate a mock Content:LocationChange message with video mime-type for the current tab (tabId = 0).
        final JSONObject message = new JSONObject();
        try {
            message.put("contentType", "video/webm");
            message.put("baseDomain", "webmfiles.org");
            message.put("type", "Content:LocationChange");
            message.put("sameDocument", false);
            message.put("userSearch", "");
            message.put("uri", getAbsoluteIpUrl("/big-buck-bunny_trailer.webm"));
            message.put("tabID", 0);
        } catch (Exception ex) {
            mAsserter.ok(false, "exception in testSaveAsPDFPathway", ex.toString());
        }

        // Mock video playback with the generated message and Content:LocationChange event.
        Tabs.getInstance().handleMessage("Content:LocationChange", message);

        // Save as pdf menu is disabled while playing video.
        mAppMenu.assertMenuItemIsDisabledAndVisible(AppMenuComponent.PageMenuItem.SAVE_AS_PDF);

        // The above mock video playback test changes Java state, but not the associated JS state.
        // Navigate to a new page so that the Java state is cleared.
        NavigationHelper.enterAndLoadUrl(StringHelper.ROBOCOP_BLANK_PAGE_01_URL);
        mToolbar.assertTitle(StringHelper.ROBOCOP_BLANK_PAGE_01_TITLE);

        // Test save as pdf functionality.
        // The following call doesn't wait for the resulting pdf but checks that no exception are thrown.
        // NOTE: save as pdf functionality must be done at the end as it is slow and cause other test operations to fail.
        mAppMenu.pressMenuItem(AppMenuComponent.PageMenuItem.SAVE_AS_PDF);
    }
}