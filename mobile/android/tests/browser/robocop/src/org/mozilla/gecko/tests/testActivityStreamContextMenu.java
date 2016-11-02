/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tests;

import android.support.design.widget.NavigationView;
import android.support.v4.app.Fragment;
import android.view.KeyEvent;
import android.view.MenuItem;
import android.view.View;

import com.robotium.solo.Condition;

import org.mozilla.gecko.R;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.home.activitystream.ActivityStream;
import org.mozilla.gecko.home.activitystream.menu.ActivityStreamContextMenu;

/**
 * This test is unfortunately closely coupled to the current implementation, however it is still
 * useful in that it tests the bookmark/history state specific menu items for correctness.
 */
public class testActivityStreamContextMenu extends BaseTest {
    public void testActivityStreamContextMenu() {
        blockForGeckoReady();

        final String testURL = "http://mozilla.org";

        BrowserDB db = BrowserDB.from(getActivity());
        db.removeHistoryEntry(getActivity().getContentResolver(), testURL);
        db.removeBookmarksWithURL(getActivity().getContentResolver(), testURL);

        testMenuForUrl(testURL, false, false);

        db.addBookmark(getActivity().getContentResolver(), "foobar", testURL);
        testMenuForUrl(testURL, true, false);

        db.updateVisitedHistory(getActivity().getContentResolver(), testURL);
        testMenuForUrl(testURL, true, true);

        db.removeBookmarksWithURL(getActivity().getContentResolver(), testURL);
        testMenuForUrl(testURL, false, true);
    }

    /**
     * Test that the menu shows the expected menu items for a given URL, and that these items have
     * the correct state.
     */
    private void testMenuForUrl(final String url, final boolean isBookmarked, final boolean isVisited) {
        final View anchor = new View(getActivity());

        final ActivityStreamContextMenu menu = ActivityStreamContextMenu.show(getActivity(), anchor, ActivityStreamContextMenu.MenuMode.HIGHLIGHT, "foobar", url, null, null, 100, 100);

        final int expectedBookmarkString;
        if (isBookmarked) {
            expectedBookmarkString = R.string.bookmark_remove;
        } else {
            expectedBookmarkString = R.string.bookmark;
        }

        final MenuItem bookmarkItem = menu.getItemByID(R.id.bookmark);
        assertMenuItemHasString(bookmarkItem, expectedBookmarkString);

        final MenuItem deleteItem = menu.getItemByID(R.id.delete);
        assertMenuItemIsVisible(deleteItem, isVisited);

        menu.dismiss();
    }

    private void assertMenuItemIsVisible(final MenuItem item, final boolean shouldBeVisible) {
        waitForCondition(new Condition() {
            @Override
            public boolean isSatisfied() {
                return (item.isVisible() == shouldBeVisible);
            }
        }, 5000);

        mAsserter.is(item.isVisible(), shouldBeVisible, "menu item \"" + item.getTitle() + "\" should be visible");
    }

    private void assertMenuItemHasString(final MenuItem item, final int stringID) {
        waitForCondition(new Condition() {
            @Override
            public boolean isSatisfied() {
                return item.isEnabled();
            }
        }, 5000);

        final String expectedTitle = getActivity().getResources().getString(stringID);
        mAsserter.is(item.getTitle(), expectedTitle, "Title does not match expected title");
    }
}

