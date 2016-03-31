/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tests;

import org.mozilla.gecko.Actions;

import com.robotium.solo.Condition;


public class testBookmarklets extends BaseTest {
    public void testBookmarklets() {
        final String url = getAbsoluteUrl(mStringHelper.ROBOCOP_BLANK_PAGE_01_URL);
        final String title = "alertBookmarklet";
        final String js = "javascript:alert(12 + 10)";
        final String expected = "22";
        boolean alerted;

        blockForGeckoReady();

        // Load a standard page so bookmarklets work
        loadUrlAndWait(url);

        // Verify that user-entered bookmarklets do *not* work
        enterUrl(js);
        mActions.sendSpecialKey(Actions.SpecialKey.ENTER);
        alerted = waitForCondition(new Condition() {
            @Override
            public boolean isSatisfied() {
                return mSolo.searchButton("OK", true) || mSolo.searchText(expected, true);
            }
        }, 3000);
        mAsserter.is(alerted, false, "Alert was not shown for user-entered bookmarklet");

        // Verify that non-user-entered bookmarklets do work
        loadUrl(js);
        alerted = waitForCondition(new Condition() {
            @Override
            public boolean isSatisfied() {
                return mSolo.searchButton("OK", true) && mSolo.searchText(expected, true);
            }
        }, 3000);
        mAsserter.is(alerted, true, "Alert was shown for bookmarklet");
    }
}
