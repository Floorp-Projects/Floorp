package org.mozilla.gecko.tests;

import org.mozilla.gecko.tests.components.AboutHomeComponent.PanelType;
import org.mozilla.gecko.tests.helpers.GeckoHelper;
import org.mozilla.gecko.tests.helpers.NavigationHelper;

/**
 * Tests the visibility of about:home after various interactions with the browser.
 */
public class testAboutHomeVisibility extends UITest {
    public void testAboutHomeVisibility() {
        GeckoHelper.blockForReady();

        // Check initial state on about:home.
        mToolbar.assertTitle(StringHelper.ABOUT_HOME_TITLE);
        mAboutHome.assertVisible()
                  .assertCurrentPanel(PanelType.TOP_SITES);

        // Go to blank 01.
        NavigationHelper.enterAndLoadUrl(StringHelper.ROBOCOP_BLANK_PAGE_01_URL);
        mToolbar.assertTitle(StringHelper.ROBOCOP_BLANK_PAGE_01_TITLE);
        mAboutHome.assertNotVisible();

        // Go to blank 02.
        NavigationHelper.enterAndLoadUrl(StringHelper.ROBOCOP_BLANK_PAGE_02_URL);
        mToolbar.assertTitle(StringHelper.ROBOCOP_BLANK_PAGE_02_TITLE);
        mAboutHome.assertNotVisible();

        // Enter editing mode, where the about:home UI should be visible.
        mToolbar.enterEditingMode();
        mAboutHome.assertVisible()
                  .assertCurrentPanel(PanelType.TOP_SITES);

        // Dismiss editing mode, where the about:home UI should be gone.
        mToolbar.dismissEditingMode();
        mAboutHome.assertNotVisible();

        // Loading about:home should show about:home again.
        NavigationHelper.enterAndLoadUrl(StringHelper.ABOUT_HOME_URL);
        mToolbar.assertTitle(StringHelper.ABOUT_HOME_TITLE);
        mAboutHome.assertVisible()
                  .assertCurrentPanel(PanelType.TOP_SITES);

        // TODO: Type in a url and assert the go button is visible.
    }
}
