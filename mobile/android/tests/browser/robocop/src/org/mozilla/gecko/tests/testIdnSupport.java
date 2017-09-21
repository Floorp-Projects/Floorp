package org.mozilla.gecko.tests;

import org.mozilla.gecko.tests.helpers.GeckoHelper;
import org.mozilla.gecko.tests.helpers.NavigationHelper;

public class testIdnSupport extends UITest {
    public void testToolbarIdnSupport() {
        GeckoHelper.blockForReady();

        mBaseHostnameUrl = "http://exämple.test/tests";
        String url = mStringHelper.ROBOCOP_BLANK_PAGE_01_URL;
        NavigationHelper.enterAndLoadUrl(url);
        mToolbar.assertTitle(url);

        mBaseHostnameUrl = "http://παράδειγμα.δοκιμή/tests";
        url = mStringHelper.ROBOCOP_BLANK_PAGE_02_URL;
        NavigationHelper.enterAndLoadUrl(url);
        mToolbar.assertTitle(url);

        mBaseHostnameUrl = "http://天気の良い日.w3c-test.org/tests";
        url = mStringHelper.ROBOCOP_BLANK_PAGE_03_URL;
        NavigationHelper.enterAndLoadUrl(url);
        mToolbar.assertTitle(url);
    }
}
