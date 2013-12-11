package org.mozilla.gecko.tests;

import static org.mozilla.gecko.tests.helpers.AssertionHelper.*;

import org.mozilla.gecko.tests.helpers.*;

/**
 * Tests that navigating through session history (ex: forward, back) sets the correct UI state.
 */
public class testSessionHistory extends UITest {
    public void testSessionHistory() {
        GeckoHelper.blockForReady();

        NavigationHelper.enterAndLoadUrl(StringHelper.ROBOCOP_BLANK_PAGE_01_URL);
        mToolbar.assertTitle(StringHelper.ROBOCOP_BLANK_PAGE_01_TITLE);

        NavigationHelper.enterAndLoadUrl(StringHelper.ROBOCOP_BLANK_PAGE_02_URL);
        mToolbar.assertTitle(StringHelper.ROBOCOP_BLANK_PAGE_02_TITLE);

        NavigationHelper.enterAndLoadUrl(StringHelper.ROBOCOP_BLANK_PAGE_03_URL);
        mToolbar.assertTitle(StringHelper.ROBOCOP_BLANK_PAGE_03_TITLE);

        NavigationHelper.goBack();
        mToolbar.assertTitle(StringHelper.ROBOCOP_BLANK_PAGE_02_TITLE);

        NavigationHelper.goBack();
        mToolbar.assertTitle(StringHelper.ROBOCOP_BLANK_PAGE_01_TITLE);

        // TODO: Implement this functionality and uncomment.
        /*
        NavigationHelper.goForward();
        mToolbar.assertTitle(StringHelper.ROBOCOP_BLANK_PAGE_02_TITLE);

        NavigationHelper.reload();
        mToolbar.assertTitle(StringHelper.ROBOCOP_BLANK_PAGE_02_TITLE);
        */
    }
}
