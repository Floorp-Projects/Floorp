/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tests;


public class testBookmarkKeyword extends AboutHomeTest {
    public void testBookmarkKeyword() {
        blockForGeckoReady();

        final String url = getAbsoluteUrl(mStringHelper.ROBOCOP_BLANK_PAGE_01_URL);
        final String keyword = "testkeyword";

        // Add a bookmark, and update it to have a keyword.
        mDatabaseHelper.addMobileBookmark(mStringHelper.ROBOCOP_BLANK_PAGE_01_TITLE, url);
        mDatabaseHelper.updateBookmark(url, mStringHelper.ROBOCOP_BLANK_PAGE_01_TITLE, keyword);

        // Enter the keyword in the urlbar.
        inputAndLoadUrl(keyword);

        // Make sure the title of the page appeared.
        verifyUrlBarTitle(mStringHelper.ROBOCOP_BLANK_PAGE_01_URL);

        // Delete the bookmark to clean up.
        mDatabaseHelper.deleteBookmark(url);
    }
}
