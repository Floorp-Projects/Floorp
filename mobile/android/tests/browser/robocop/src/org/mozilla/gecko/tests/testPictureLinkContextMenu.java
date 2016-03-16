/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tests;

public class testPictureLinkContextMenu extends ContentContextMenuTest {

    // Test website strings
    private static String PICTURE_PAGE_URL;
    private static String BLANK_PAGE_URL;
    private static String PICTURE_URL;
    private static final String tabs [] = { "Image", "Link" };
    private static final String photoMenuItems [] = { "Copy Image Location", "Share Image", "View Image", "Set Image As", "Save Image" };
    private static final String imageTitle = "^Image$";

    public void testPictureLinkContextMenu() {
        final String PICTURE_PAGE_TITLE = mStringHelper.ROBOCOP_PICTURE_LINK_TITLE;
        final String linkMenuItems [] = mStringHelper.CONTEXT_MENU_ITEMS_IN_NORMAL_TAB;

        blockForGeckoReady();

        PICTURE_PAGE_URL=getAbsoluteUrl(mStringHelper.ROBOCOP_PICTURE_LINK_URL);
        BLANK_PAGE_URL=getAbsoluteUrl(mStringHelper.ROBOCOP_BLANK_PAGE_02_URL);
        PICTURE_URL=getAbsoluteUrl(mStringHelper.ROBOCOP_PICTURE_URL);
        loadAndPaint(PICTURE_PAGE_URL);
        verifyUrlInContentDescription(PICTURE_PAGE_URL);

        switchTabs(imageTitle);
        verifyContextMenuItems(photoMenuItems);
        verifyTabs(tabs);
        switchTabs(imageTitle);
        verifyCopyOption(photoMenuItems[0], "Firefox.jpg"); // Test the "Copy Image Location" option
        switchTabs(imageTitle);
        verifyShareOption(photoMenuItems[1], PICTURE_PAGE_TITLE); // Test the "Share Image" option
        switchTabs(imageTitle);
        verifyViewImageOption(photoMenuItems[2], PICTURE_URL, PICTURE_PAGE_TITLE); // Test the "View Image" option

        verifyContextMenuItems(linkMenuItems);
        openTabFromContextMenu(linkMenuItems[0],2); // Test the "Open in New Tab" option - expecting 2 tabs: the original and the new one
        openTabFromContextMenu(linkMenuItems[1],2); // Test the "Open in Private Tab" option - expecting only 2 tabs in normal mode
        verifyCopyOption(linkMenuItems[2], BLANK_PAGE_URL); // Test the "Copy Link" option
        verifyShareOption(linkMenuItems[3], PICTURE_PAGE_TITLE); // Test the "Share Link" option
        verifyBookmarkLinkOption(linkMenuItems[4],BLANK_PAGE_URL); // Test the "Bookmark Link" option
    }

    @Override
    public void tearDown() throws Exception {
        mDatabaseHelper.deleteBookmark(BLANK_PAGE_URL);
        super.tearDown();
    }
}
