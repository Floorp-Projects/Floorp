/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tests;

public class testLinkContextMenu extends ContentContextMenuTest {

    // Test website strings
    private static String LINK_PAGE_URL;
    private static String BLANK_PAGE_URL;
    private static final String LINK_PAGE_TITLE = "Big Link";

    public void testLinkContextMenu() {
        final String linkMenuItems [] = mStringHelper.CONTEXT_MENU_ITEMS_IN_NORMAL_TAB;

        blockForGeckoReady();

        LINK_PAGE_URL=getAbsoluteUrl(mStringHelper.ROBOCOP_BIG_LINK_URL);
        BLANK_PAGE_URL=getAbsoluteUrl(mStringHelper.ROBOCOP_BLANK_PAGE_01_URL);
        loadUrlAndWait(LINK_PAGE_URL);
        waitForText(LINK_PAGE_TITLE);

        verifyContextMenuItems(linkMenuItems); // Verify context menu items are correct
        openTabFromContextMenu(linkMenuItems[0],2); // Test the "Open in New Tab" option - expecting 2 tabs: the original and the new one
        openTabFromContextMenu(linkMenuItems[1],2); // Test the "Open in Private Tab" option - expecting only 2 tabs in normal mode
        verifyCopyOption(linkMenuItems[2], BLANK_PAGE_URL); // Test the "Copy Link" option
        verifyShareOption(linkMenuItems[3], LINK_PAGE_TITLE); // Test the "Share Link" option
        verifyBookmarkLinkOption(linkMenuItems[4], BLANK_PAGE_URL); // Test the "Bookmark Link" option
    }

    @Override
    public void tearDown() throws Exception {
        mDatabaseHelper.deleteBookmark(BLANK_PAGE_URL);
        super.tearDown();
    }
}
