/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tests;

import android.view.MenuItem;
import android.view.View;

import com.robotium.solo.Condition;

import org.mozilla.gecko.R;
import org.mozilla.gecko.activitystream.ActivityStream;
import org.mozilla.gecko.activitystream.ActivityStreamTelemetry;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.home.activitystream.menu.ActivityStreamContextMenu;

/**
 * This test is unfortunately closely coupled to the current implementation, however it is still
 * useful in that it tests the bookmark/history/pinned state specific menu items for correctness.
 */
public class testActivityStreamContextMenu extends BaseTest {
    public void testActivityStreamContextMenu() {
        blockForGeckoReady();

        final String testURL = "http://mozilla.org";

        BrowserDB db = BrowserDB.from(getActivity());
        db.removeHistoryEntry(getActivity().getContentResolver(), testURL);
        db.removeBookmarksWithURL(getActivity().getContentResolver(), testURL);

        testMenuForUrl(testURL, null, false, null, false, false);

        db.addBookmark(getActivity().getContentResolver(), "foobar", testURL);
        testMenuForUrl(testURL, null, true, null, false, false);
        testMenuForUrl(testURL, null, true, Boolean.FALSE, false, false);
        testMenuForUrl(testURL, Boolean.TRUE, true, null, false, false);
        testMenuForUrl(testURL, Boolean.TRUE, true, Boolean.FALSE, false, false);

        db.updateVisitedHistory(getActivity().getContentResolver(), testURL);
        testMenuForUrl(testURL, null, true, null, false, true);
        testMenuForUrl(testURL, null, true, Boolean.FALSE, false, true);
        testMenuForUrl(testURL, Boolean.TRUE, true, null, false, true);
        testMenuForUrl(testURL, Boolean.TRUE, true, Boolean.FALSE, false, true);

        db.removeBookmarksWithURL(getActivity().getContentResolver(), testURL);
        testMenuForUrl(testURL, null, false, null, false, true);
        testMenuForUrl(testURL, Boolean.FALSE, false, null, false, true);

        db.pinSiteForAS(getActivity().getContentResolver(), testURL, "test title");
        testMenuForUrl(testURL, null, false, null, true, true);
        testMenuForUrl(testURL, null, false, Boolean.TRUE, true, true);

        db.addBookmark(getActivity().getContentResolver(), "foobar", testURL);
        testMenuForUrl(testURL, null, true, null, true, true);
        testMenuForUrl(testURL, Boolean.TRUE, true, Boolean.TRUE, true, true);
    }

    /**
     * Test that the menu shows the expected menu items for a given URL, and that these items have
     * the correct state.
     */
    private void testMenuForUrl(final String url, final Boolean isBookmarkedKnownState, final boolean isBookmarked, final Boolean isPinnedKnownState, final boolean isPinned, final boolean isVisited) {
        final View anchor = new View(getActivity());

        final ActivityStreamContextMenu menu = ActivityStreamContextMenu.show(
                getActivity(), anchor, ActivityStreamTelemetry.Extras.builder(), ActivityStreamContextMenu.MenuMode.HIGHLIGHT, "foobar", url, isBookmarkedKnownState, isPinnedKnownState, null, null, 100, 100);

        final int expectedBookmarkString;
        if (isBookmarked) {
            expectedBookmarkString = R.string.bookmark_remove;
        } else {
            expectedBookmarkString = R.string.bookmark;
        }

        final int expectedPinnedString;
        if (isPinned) {
            expectedPinnedString = R.string.contextmenu_top_sites_unpin;
        } else {
            expectedPinnedString = R.string.contextmenu_top_sites_pin;
        }

        final MenuItem pinItem = menu.getItemByID(R.id.pin);
        assertMenuItemHasString(pinItem, expectedPinnedString);

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

