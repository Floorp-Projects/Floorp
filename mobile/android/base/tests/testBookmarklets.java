package org.mozilla.gecko.tests;

import org.mozilla.gecko.*;
import android.database.Cursor;

import android.widget.ListView;


public class testBookmarklets extends AboutHomeTest {
    @Override
    protected int getTestType() {
        return TEST_MOCHITEST;
    }

    public void testBookmarklets() {
        final String url = getAbsoluteUrl(StringHelper.ROBOCOP_BLANK_PAGE_01_URL);
        final String title = "alertBookmarklet";
        final String js = "javascript:alert(12 + .34)";
        boolean alerted;

        blockForGeckoReady();

        // load a standard page so bookmarklets work
        inputAndLoadUrl(url);
        verifyPageTitle(StringHelper.ROBOCOP_BLANK_PAGE_01_TITLE); // Waiting for page title to ensure the page is loaded

        // verify that user-entered bookmarklets do *not* work
        enterUrl(js);
        mActions.sendSpecialKey(Actions.SpecialKey.ENTER);
        alerted = waitForTest(new BooleanTest() {
            @Override
            public boolean test() {
                return mSolo.searchButton("OK", true) || mSolo.searchText("12.34", true);
            }
        }, 3000);
        mAsserter.is(alerted, false, "Alert was not shown for user-entered bookmarklet");

        // add the bookmarklet to the database. there's currently no way to
        // add this using the UI, so we go through the content provider.
        mDatabaseHelper.addOrUpdateMobileBookmark(title, js);

        // Open about:home in the Bookmarks page
        openAboutHomeTab(AboutHomeTabs.BOOKMARKS);

        ListView bookmarks = findListViewWithTag("bookmarks");
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

        alerted = waitForTest(new BooleanTest() {
            @Override
            public boolean test() {
                return mSolo.searchButton("OK", true) && mSolo.searchText("12.34", true);
            }
        }, 3000);
        mAsserter.is(alerted, true, "Alert was shown for clicked bookmarklet");

        // remove the bookmarklet
        mDatabaseHelper.deleteBookmark(js);
    }
}
