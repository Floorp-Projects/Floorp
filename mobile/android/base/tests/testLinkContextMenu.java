package org.mozilla.gecko.tests;

import org.mozilla.gecko.*;

public class testLinkContextMenu extends ContentContextMenuTest {

    // Test website strings
    private static String LINK_PAGE_URL;
    private static String BLANK_PAGE_URL;
    private static final String LINK_PAGE_TITLE = "Big Link";
    private static final String linkMenuItems [] = { "Open Link in New Tab", "Open Link in Private Tab", "Copy Link", "Share Link", "Bookmark Link"};

    @Override
    protected int getTestType() {
        return TEST_MOCHITEST;
    }

    public void testLinkContextMenu() {
        blockForGeckoReady();

        LINK_PAGE_URL=getAbsoluteUrl("/robocop/robocop_big_link.html");
        BLANK_PAGE_URL=getAbsoluteUrl("/robocop/robocop_blank_01.html");
        inputAndLoadUrl(LINK_PAGE_URL);
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
