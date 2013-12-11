package org.mozilla.gecko.tests;

import org.mozilla.gecko.*;
import org.mozilla.gecko.util.Clipboard;

import android.content.ContentResolver;
import android.util.DisplayMetrics;


/**
 * This class covers interactions with the context menu opened from web content
 */
abstract class ContentContextMenuTest extends PixelTest {
    private static final int MAX_TEST_TIMEOUT = 10000;

    // This method opens the context menu of any web content. It assumes that the page is already loaded
    protected void openWebContentContextMenu(String waitText) {
        DisplayMetrics dm = new DisplayMetrics();
        getActivity().getWindowManager().getDefaultDisplay().getMetrics(dm);

        // The web content we are trying to open the context menu for should be positioned at the top of the page, at least 60px heigh and aligned to the middle
        float top = mDriver.getGeckoTop() + 30 * dm.density;
        float left = mDriver.getGeckoLeft() + mDriver.getGeckoWidth() / 2;

        mAsserter.dumpLog("long-clicking at "+left+", "+top);
        mSolo.clickLongOnScreen(left, top);
        waitForText(waitText);
    }

    protected void verifyContextMenuItems(String[] items) {
        // Test that the menu items are displayed
        openWebContentContextMenu(items[0]);
        for (String option:items) {
            mAsserter.ok(mSolo.searchText(option), "Checking that the option: " + option + " is available", "The option is available");
        }
    }

    protected void openTabFromContextMenu(String contextMenuOption, int expectedTabCount) {
        if (!mSolo.searchText(contextMenuOption)) {
            openWebContentContextMenu(contextMenuOption); // Open the context menu if it is not already
        }
        Actions.EventExpecter tabEventExpecter = mActions.expectGeckoEvent("Tab:Added");
        mSolo.clickOnText(contextMenuOption);
        tabEventExpecter.blockForEvent();
        tabEventExpecter.unregisterListener();
        verifyTabCount(expectedTabCount);
    }

    protected void verifyCopyOption(String copyOption, final String copiedText) {
        if (!mSolo.searchText(copyOption)) {
            openWebContentContextMenu(copyOption); // Open the context menu if it is not already
        }
        mSolo.clickOnText(copyOption);
        boolean correctText = waitForTest(new BooleanTest() {
            @Override
            public boolean test() {
                final String clipboardText = Clipboard.getText();
                mAsserter.dumpLog("Clipboard text = " + clipboardText + " , expected text = " + copiedText);
                return clipboardText.contains(copiedText);
            }
        }, MAX_TEST_TIMEOUT);
        mAsserter.ok(correctText, "Checking if the text is correctly copied", "The text was correctly copied");
    }



    protected void verifyShareOption(String shareOption, String pageTitle) {
        waitForText(pageTitle);
        openWebContentContextMenu(shareOption);
        mSolo.clickOnText(shareOption);
        mAsserter.ok(waitForText("Share via"), "Checking that the share pop-up is displayed", "The pop-up has been displayed");

        // Close the Share Link option menu and wait for the page to be focused again
        mActions.sendSpecialKey(Actions.SpecialKey.BACK);
        waitForText(pageTitle);
    }

    protected void verifyBookmarkLinkOption(String bookmarkOption, String link) {
        openWebContentContextMenu(bookmarkOption);
        mSolo.clickOnText(bookmarkOption);
        mAsserter.ok(waitForText("Bookmark added"), "Waiting for the Bookmark added toaster notification", "The notification has been displayed");
        mAsserter.ok(mDatabaseHelper.isBookmark(link), "Checking if the link has been added as a bookmark", "The link has been bookmarked");
    }
}
