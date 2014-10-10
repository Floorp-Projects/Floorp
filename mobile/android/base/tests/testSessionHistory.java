package org.mozilla.gecko.tests;

import org.mozilla.gecko.tests.helpers.GeckoHelper;
import org.mozilla.gecko.tests.helpers.NavigationHelper;

/**
 * Tests that navigating through session history (ex: forward, back) sets the correct UI state.
 */
public class testSessionHistory extends UITest {
    public void testSessionHistory() {
        GeckoHelper.blockForReady();

        String url = StringHelper.ROBOCOP_BLANK_PAGE_01_URL;
        NavigationHelper.enterAndLoadUrl(url);
        mToolbar.assertTitle(StringHelper.ROBOCOP_BLANK_PAGE_01_TITLE, url);

        url = StringHelper.ROBOCOP_BLANK_PAGE_02_URL;
        NavigationHelper.enterAndLoadUrl(url);
        mToolbar.assertTitle(StringHelper.ROBOCOP_BLANK_PAGE_02_TITLE, url);

        url = StringHelper.ROBOCOP_BLANK_PAGE_03_URL;
        NavigationHelper.enterAndLoadUrl(url);
        mToolbar.assertTitle(StringHelper.ROBOCOP_BLANK_PAGE_03_TITLE, url);

        NavigationHelper.goBack();
        mToolbar.assertTitle(StringHelper.ROBOCOP_BLANK_PAGE_02_TITLE,
                StringHelper.ROBOCOP_BLANK_PAGE_02_URL);

        NavigationHelper.goBack();
        mToolbar.assertTitle(StringHelper.ROBOCOP_BLANK_PAGE_01_TITLE,
                StringHelper.ROBOCOP_BLANK_PAGE_01_URL);

        NavigationHelper.goForward();
        mToolbar.assertTitle(StringHelper.ROBOCOP_BLANK_PAGE_02_TITLE,
                StringHelper.ROBOCOP_BLANK_PAGE_02_URL);

        NavigationHelper.reload();
        mToolbar.assertTitle(StringHelper.ROBOCOP_BLANK_PAGE_02_TITLE,
                StringHelper.ROBOCOP_BLANK_PAGE_02_URL);
    }
}
