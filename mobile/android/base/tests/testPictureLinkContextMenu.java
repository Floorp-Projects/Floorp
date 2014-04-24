package org.mozilla.gecko.tests;


public class testPictureLinkContextMenu extends ContentContextMenuTest {

    // Test website strings
    private static String PICTURE_PAGE_URL;
    private static String BLANK_PAGE_URL;
    private static final String PICTURE_PAGE_TITLE = "Picture Link";
    private static final String tabs [] = { "Image", "Link" };
    private static final String photoMenuItems [] = { "Copy Image Location", "Share Image", "Set Image As", "Save Image" };
    private static final String linkMenuItems [] = { "Open Link in New Tab", "Open Link in Private Tab", "Copy Link", "Share Link", "Bookmark Link"};
    private static final String imageTitle = "^Image$";

    public void testPictureLinkContextMenu() {
        blockForGeckoReady();

        PICTURE_PAGE_URL=getAbsoluteUrl("/robocop/robocop_picture_link.html");
        BLANK_PAGE_URL=getAbsoluteUrl("/robocop/robocop_blank_02.html");
        loadAndPaint(PICTURE_PAGE_URL);
        verifyPageTitle(PICTURE_PAGE_TITLE);

        switchTabs(imageTitle);
        verifyContextMenuItems(photoMenuItems);
        verifyTabs(tabs);
        switchTabs(imageTitle);
        verifyCopyOption(photoMenuItems[0], "Firefox.jpg"); // Test the "Copy Image Location" option
        switchTabs(imageTitle);
        verifyShareOption(photoMenuItems[1], PICTURE_PAGE_TITLE); // Test the "Share Image" option

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
