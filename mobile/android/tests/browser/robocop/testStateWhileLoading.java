package org.mozilla.gecko.tests;

import org.mozilla.gecko.tests.helpers.DeviceHelper;
import org.mozilla.gecko.tests.helpers.GeckoClickHelper;
import org.mozilla.gecko.tests.helpers.GeckoHelper;
import org.mozilla.gecko.tests.helpers.NavigationHelper;
import org.mozilla.gecko.tests.helpers.WaitHelper;

/**
 * This test ensures the back/forward state is correct when switching to loading pages
 * to prevent regressions like Bug 1124190.
 */
public class testStateWhileLoading extends UITest {
    public void testStateWhileLoading() {
        if (!DeviceHelper.isTablet()) {
            // This test case only covers tablets currently.
            return;
        }

        GeckoHelper.blockForReady();

        NavigationHelper.enterAndLoadUrl(mStringHelper.ROBOCOP_LINK_TO_SLOW_LOADING);

        GeckoClickHelper.openCentralizedLinkInNewTab();

        WaitHelper.waitForPageLoad(new Runnable() {
            @Override
            public void run() {
                mTabStrip.switchToTab(1);

                // Assert that the state of the back button is correct
                // after switching to the new (still loading) tab.
                mToolbar.assertBackButtonIsNotEnabled();
            }
        });

        // Assert that the state of the back button is still correct after the page has loaded.
        mToolbar.assertBackButtonIsNotEnabled();
    }
}
