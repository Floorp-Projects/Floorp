package org.mozilla.gecko.tests;

import org.mozilla.gecko.tests.components.AppMenuComponent;
import org.mozilla.gecko.tests.helpers.GeckoHelper;
import org.mozilla.gecko.tests.helpers.NavigationHelper;
import org.mozilla.gecko.tests.helpers.WaitHelper;

public class testViewPageSource extends UITest {
    public void testViewPageSource() {
        GeckoHelper.blockForReady();

        // Page menu should be disabled in about:home.
        mAppMenu.assertMenuItemIsDisabledAndVisible(AppMenuComponent.PageMenuItem.VIEW_PAGE_SOURCE);

        final String testUrl = NavigationHelper.adjustUrl(getStringHelper().ABOUT_FIREFOX_URL);
        NavigationHelper.enterAndLoadUrl(testUrl);

        WaitHelper.waitForPageLoad(new Runnable() {
            @Override
            public void run() {
                mAppMenu.pressMenuItem(AppMenuComponent.PageMenuItem.VIEW_PAGE_SOURCE);
            }
        });

        final String viewSourceUrl = "view-source:" + testUrl;
        mToolbar.assertTitle(viewSourceUrl);
    }
}
