/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tests;

import org.mozilla.gecko.Actions;
import org.mozilla.gecko.util.Clipboard;

import android.util.DisplayMetrics;

import com.jayway.android.robotium.solo.Condition;

/**
 * This class covers interactions with the context menu opened from web content
 */
abstract class ContentContextMenuTest extends PixelTest {
    private static final int MAX_TEST_TIMEOUT = 30000; // 30 seconds (worst case)

    // This method opens the context menu of any web content. It assumes that the page is already loaded
    protected void openWebContentContextMenu(String waitText) {
        DisplayMetrics dm = new DisplayMetrics();
        getActivity().getWindowManager().getDefaultDisplay().getMetrics(dm);

        // The web content we are trying to open the context menu for should be positioned at the top of the page, at least 60px high and aligned to the middle
        float top = mDriver.getGeckoTop() + 30 * dm.density;
        float left = mDriver.getGeckoLeft() + mDriver.getGeckoWidth() / 2;

        mAsserter.dumpLog("long-clicking at "+left+", "+top);
        mSolo.clickLongOnScreen(left, top);
        waitForText(waitText);
    }

    protected void verifyContextMenuItems(String[] items) {
        // Test that the menu items are displayed
        if (!mSolo.searchText(items[0])) {
            openWebContentContextMenu(items[0]); // Open the context menu if it is not already
        }

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

    protected void verifyTabs(String[] items) {
        if (!mSolo.searchText(items[0])) {
            openWebContentContextMenu(items[0]);
        }

        for (String option:items) {
            mAsserter.ok(mSolo.searchText(option), "Checking that the option: " + option + " is available", "The option is available");
        }
    }

    protected void switchTabs(String tab) {
        if (!mSolo.searchText(tab)) {
            openWebContentContextMenu(tab);
        }
        mSolo.clickOnText(tab);
    }


    protected void verifyCopyOption(String copyOption, final String copiedText) {
        if (!mSolo.searchText(copyOption)) {
            openWebContentContextMenu(copyOption); // Open the context menu if it is not already
        }
        mSolo.clickOnText(copyOption);
        boolean correctText = waitForCondition(new Condition() {
            @Override
            public boolean isSatisfied() {
                final String clipboardText = Clipboard.getText();
                mAsserter.dumpLog("Clipboard text = " + clipboardText + " , expected text = " + copiedText);
                return clipboardText.contains(copiedText);
            }
        }, MAX_TEST_TIMEOUT);
        mAsserter.ok(correctText, "Checking if the text is correctly copied", "The text was correctly copied");
    }



    protected void verifyShareOption(String shareOption, String pageTitle) {
        waitForText(pageTitle); // Even if this fails, it won't assert
        if (!mSolo.searchText(shareOption)) {
            openWebContentContextMenu(shareOption); // Open the context menu if it is not already
        }
        mSolo.clickOnText(shareOption);
        mAsserter.ok(waitForText(shareOption), "Checking that the share pop-up is displayed", "The pop-up has been displayed");

        // Close the Share Link option menu and wait for the page to be focused again
        mActions.sendSpecialKey(Actions.SpecialKey.BACK);
        waitForText(pageTitle);
    }

    protected void verifyBookmarkLinkOption(String bookmarkOption, String link) {
        if (!mSolo.searchText(bookmarkOption)) {
            openWebContentContextMenu(bookmarkOption); // Open the context menu if it is not already
        }
        mSolo.clickOnText(bookmarkOption);
        mAsserter.ok(waitForText("Bookmark added"), "Waiting for the Bookmark added toaster notification", "The notification has been displayed");
        mAsserter.ok(mDatabaseHelper.isBookmark(link), "Checking if the link has been added as a bookmark", "The link has been bookmarked");
    }
}
