package org.mozilla.gecko.tests;

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

        // Navigate to a page to test save as pdf functionality.
        NavigationHelper.enterAndLoadUrl(StringHelper.ROBOCOP_BLANK_PAGE_01_URL);
        mToolbar.assertTitle(StringHelper.ROBOCOP_BLANK_PAGE_01_TITLE);

        // Test save as pdf functionality.
        // The following call doesn't wait for the resulting pdf but checks that no exception are thrown.
        mAppMenu.pressMenuItem(AppMenuComponent.PageMenuItem.SAVE_AS_PDF);
    }
}