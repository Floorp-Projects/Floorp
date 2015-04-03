/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tests;

import org.mozilla.gecko.Actions;
import org.mozilla.gecko.home.HomePager;

import android.database.Cursor;
import android.widget.ListView;

import com.jayway.android.robotium.solo.Condition;


public class testBookmarklets extends AboutHomeTest {
    public void testBookmarklets() {
        final String url = getAbsoluteUrl(mStringHelper.ROBOCOP_BLANK_PAGE_01_URL);
        final String title = "alertBookmarklet";
        final String js = "javascript:alert(12 + .34)";
        boolean alerted;

        blockForGeckoReady();

        // load a standard page so bookmarklets work
        inputAndLoadUrl(url);
        // Waiting for page title to ensure the page is loaded
        verifyUrlBarTitle(mStringHelper.ROBOCOP_BLANK_PAGE_01_URL);

        // verify that user-entered bookmarklets do *not* work
        enterUrl(js);
        mActions.sendSpecialKey(Actions.SpecialKey.ENTER);
        alerted = waitForCondition(new Condition() {
            @Override
            public boolean isSatisfied() {
                return mSolo.searchButton("OK", true) || mSolo.searchText("12.34", true);
            }
        }, 3000);
        mAsserter.is(alerted, false, "Alert was not shown for user-entered bookmarklet");

        // add the bookmarklet to the database. there's currently no way to
        // add this using the UI, so we go through the content provider.
        mDatabaseHelper.addOrUpdateMobileBookmark(title, js);

        // Open about:home in the Bookmarks page
        openAboutHomeTab(AboutHomeTabs.BOOKMARKS);

        ListView bookmarks = findListViewWithTag(HomePager.LIST_TAG_BOOKMARKS);
        mAsserter.is(waitForNonEmptyListToLoad(bookmarks), true, "list is properly loaded");

        int width = mDriver.getGeckoWidth();
        int height = mDriver.getGeckoHeight();

        // Scroll down so that the bookmarks list has more items on screen.
        mActions.drag(width / 2, width / 2, height - 10, height / 2);

        // Verify that bookmarklets clicked in awesomescreen work
        boolean found = false;
        for (int i = bookmarks.getHeaderViewsCount(); i < bookmarks.getAdapter().getCount(); i++) {
            Cursor c = (Cursor)bookmarks.getItemAtPosition(i);
            String aUrl = c.getString(c.getColumnIndexOrThrow("url"));
            if (aUrl.equals(js)) {
                found = true;
                mAsserter.is(1, 1, "Found bookmarklet added to bookmarks: " + js);
                mSolo.clickOnView(bookmarks.getChildAt(i));
            }
        }

        if (!found) {
            mAsserter.is(found, true, "Found the bookmark: " + js + " and clicked on it");
        }

        alerted = waitForCondition(new Condition() {
            @Override
            public boolean isSatisfied() {
                return mSolo.searchButton("OK", true) && mSolo.searchText("12.34", true);
            }
        }, 3000);
        mAsserter.is(alerted, true, "Alert was shown for clicked bookmarklet");

        // remove the bookmarklet
        mDatabaseHelper.deleteBookmark(js);
    }
}
