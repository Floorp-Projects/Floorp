package org.mozilla.gecko.tests;

import org.mozilla.gecko.home.HomeConfig;
import org.mozilla.gecko.home.HomeConfig.PanelType;
import org.mozilla.gecko.tests.helpers.GeckoHelper;
import org.mozilla.gecko.tests.helpers.NavigationHelper;

/**
 * Tests the visibility of about:home after various interactions with the browser.
 */
public class testAboutHomeVisibility extends UITest {
    public void testAboutHomeVisibility() {
        GeckoHelper.blockForReady();

        // Check initial state on about:home.
        mToolbar.assertTitle(StringHelper.ABOUT_HOME_TITLE, StringHelper.ABOUT_HOME_URL);
        mAboutHome.assertVisible()
                  .assertCurrentPanel(PanelType.TOP_SITES);

        // Go to blank 01.
        NavigationHelper.enterAndLoadUrl(StringHelper.ROBOCOP_BLANK_PAGE_01_URL);
        mToolbar.assertTitle(StringHelper.ROBOCOP_BLANK_PAGE_01_TITLE,
                StringHelper.ROBOCOP_BLANK_PAGE_01_URL);
        mAboutHome.assertNotVisible();

        // Go to blank 02.
        NavigationHelper.enterAndLoadUrl(StringHelper.ROBOCOP_BLANK_PAGE_02_URL);
        mToolbar.assertTitle(StringHelper.ROBOCOP_BLANK_PAGE_02_TITLE,
                StringHelper.ROBOCOP_BLANK_PAGE_02_URL);
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
        mToolbar.assertTitle(StringHelper.ABOUT_HOME_TITLE, StringHelper.ABOUT_HOME_URL);
        mAboutHome.assertVisible()
                  .assertCurrentPanel(PanelType.TOP_SITES);

        // We can navigate to about:home panels by panel UUID.
        mAboutHome.navigateToBuiltinPanelType(PanelType.BOOKMARKS)
                  .assertVisible()
                  .assertCurrentPanel(PanelType.BOOKMARKS);
        mAboutHome.navigateToBuiltinPanelType(PanelType.HISTORY)
                  .assertVisible()
                  .assertCurrentPanel(PanelType.HISTORY);
    }
}
