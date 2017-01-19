/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tests;

import android.content.ContentResolver;
import android.content.ContentValues;
import android.content.Context;
import android.database.Cursor;
import android.net.Uri;
import android.view.MenuItem;
import android.view.View;

import com.robotium.solo.Condition;

import org.mozilla.gecko.GeckoProfile;
import org.mozilla.gecko.R;
import org.mozilla.gecko.activitystream.ActivityStreamTelemetry;
import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.db.DBUtils;
import org.mozilla.gecko.home.activitystream.menu.ActivityStreamContextMenu;
import org.mozilla.gecko.home.activitystream.model.Highlight;
import org.mozilla.gecko.home.activitystream.model.Item;
import org.mozilla.gecko.home.activitystream.model.TopSite;

/**
 * This test is unfortunately closely coupled to the current implementation, however it is still
 * useful in that it tests the bookmark/history/pinned state specific menu items for correctness.
 */
public class testActivityStreamContextMenu extends BaseTest {
    private static final String TEST_URL = "http://example.com/test/url";

    private BrowserDB db;
    private ContentResolver contentResolver;

    @Override
    public void setUp() throws Exception {
        super.setUp();

        cleanup();
    }

    @Override
    public void tearDown() throws Exception {
        super.tearDown();

        cleanup();
    }

    /**
     * Test the context menu for an item in all configuration states (highlight / top site).
     */
    public void testActivityStreamContextMenu() {
        blockForGeckoReady();

        testActivityStreamContextMenuForHighlights();

        cleanup();

        testActivityStreamContextMenuForTopSites();
    }

    /**
     * Test the activity stream context menu for highlights.
     *
     * Not all combinations are possible with a highlight -> A highlight is either a bookmark or
     * a history entry.
     *
     *    B
     *    O
     *    O
     *    K V
     *  P M I
     *  I A S
     *  N R I
     *  N K T
     *  E E E
     *  D D D
     *  -----
     *  0 0 0 - Impossible
     *  0 0 1 - Tested
     *  0 1 0 - Tested
     *  0 1 1 - Tested
     *  1 0 0 - Impossible
     *  1 0 1 - Tested
     *  1 1 0 - Tested
     *  1 1 1 - Tested
     */
    private void testActivityStreamContextMenuForHighlights() {
        // (001) Insert a visit an hour ago - This will make sure that the URL shows up as highlight
        insertVisit(TEST_URL, System.currentTimeMillis() - 60 * 1000 * 1000);

        mAsserter.info("ContextMenu", "Highlight (Visited)");
        testMenuForHighlight(TEST_URL,
                /* bookmarked */ false,
                /* pinned */ false,
                /* visited */ true);

        // (011) Now let's insert a bookmark and test the menu again
        db.addBookmark(contentResolver, "foobar", TEST_URL);

        mAsserter.info("ContextMenu", "Highlight (Bookmarked, Visited)");
        testMenuForHighlight(TEST_URL,
                /* bookmarked */ true,
                /* pinned */ false,
                /* visited */ true);

        // (111) And now let's pin the page too.
        db.pinSiteForAS(contentResolver, TEST_URL, "barfoo");

        mAsserter.info("ContextMenu", "Highlight (Bookmarked, Pinned, Visited)");
        testMenuForHighlight(TEST_URL,
                /* bookmarked */ true,
                /* pinned */ true,
                /* visited */ true);

        // (110) Now let's get rid of the visit again
        db.removeHistoryEntry(contentResolver, TEST_URL);

        mAsserter.info("ContextMenu", "Highlight (Bookmarked, Pinned)");
        testMenuForHighlight(TEST_URL,
                /* bookmarked */ true,
                /* pinned */ true,
                /* visited */ false);

        // (010) Unpin the site again
        db.unpinSiteForAS(contentResolver, TEST_URL);

        mAsserter.info("ContextMenu", "Highlight (Bookmarked)");
        testMenuForHighlight(TEST_URL,
                /* bookmarked */ true,
                /* pinned */ false,
                /* visited */ false);

        // (101) Remove the bookmark, but visit and pin the site
        db.removeBookmarksWithURL(contentResolver, TEST_URL);
        db.pinSiteForAS(contentResolver, TEST_URL, "barfoo");
        insertVisit(TEST_URL, System.currentTimeMillis() - 60 * 1000 * 1000);

        mAsserter.info("ContextMenu", "Highlight (Pinned, Visited)");
        testMenuForHighlight(TEST_URL,
                /* bookmarked */ false,
                /* pinned */ true,
                /* visited */ true);
    }

    /**
     * Test the activity stream context menu for top sites.
     *
     * Not all combinations are possible with a top site -> A top site needs to be visited or
     * pinned.
     *
     *    B
     *    O
     *    O
     *    K V
     *  P M I
     *  I A S
     *  N R I
     *  N K T
     *  E E E
     *  D D D
     *  -----
     *  0 0 0 - Impossible
     *  0 0 1 - Tested
     *  0 1 0 - Impossible
     *  0 1 1 - Tested
     *  1 0 0 - Tested
     *  1 0 1 - Tested
     *  1 1 0 - Tested
     *  1 1 1 - Tested
     */
    private void testActivityStreamContextMenuForTopSites() {
        // (001) First insert a visit for the URL
        insertVisit(TEST_URL, System.currentTimeMillis());

        insertVisit(TEST_URL, System.currentTimeMillis());
        insertVisit(TEST_URL, System.currentTimeMillis());
        insertVisit(TEST_URL, System.currentTimeMillis());

        mAsserter.info("ContextMenu", "Top Site (Visited)");
        testMenuForTopSite(TEST_URL,
                /* bookmarked */ false,
                /* pinned */ false,
                /* visited */ true);

        // (011) Now bookmark the URL
        db.addBookmark(contentResolver, "foobar", TEST_URL);

        mAsserter.info("ContextMenu", "Top Site (Bookmarked, Visited)");
        testMenuForTopSite(TEST_URL,
                /* bookmarked */ true,
                /* pinned */ false,
                /* visited */ true);

        // (111) And let's pin the site too
        db.pinSiteForAS(contentResolver, TEST_URL, "foobar");

        mAsserter.info("ContextMenu", "Top Site (Pinned, Bookmarked, Visited)");
        testMenuForTopSite(TEST_URL,
                /* bookmarked */ true,
                /* pinned */ true,
                /* visited */ true);

        // (110) Now remove the history entry.
        db.removeHistoryEntry(contentResolver, TEST_URL);

        mAsserter.info("ContextMenu", "Top Site (Pinned, Bookmarked)");
        testMenuForTopSite(TEST_URL,
                /* bookmarked */ true,
                /* pinned */ true,
                /* visited */ false);

        // (100) Let's remove the bookmark too.
        db.removeBookmarksWithURL(contentResolver, TEST_URL);

        mAsserter.info("ContextMenu", "Top Site (Pinned)");
        testMenuForTopSite(TEST_URL,
                /* bookmarked */ false,
                /* pinned */ true,
                /* visited */ false);

        // (101) And finally let's visit the URL once more
        insertVisit(TEST_URL, System.currentTimeMillis());

        testMenuForTopSite(TEST_URL,
                /* bookmarked */ false,
                /* pinned */ true,
                /* visited */ true);
    }

    private void testMenuForHighlight(String url, boolean bookmarked, boolean pinned, boolean visited) {
        final Highlight highlight = findHighlightByUrl(url);

        testMenuForItem(highlight, bookmarked, pinned, visited);
    }

    private void testMenuForTopSite(String url, boolean bookmarked, boolean pinned, boolean visited) {
        final TopSite topSite = findTopSiteByUrl(url);

        testMenuForItem(topSite, bookmarked, pinned, visited);
    }

    /**
     * Test that the menu shows the expected menu items for a given URL, and that these items have
     * the correct state.
     */
    private void testMenuForItem(Item item, boolean bookmarked, boolean pinned, boolean visited) {
        final View anchor = new View(getActivity());
        final ActivityStreamContextMenu menu = ActivityStreamContextMenu.show(
                getActivity(), anchor, ActivityStreamTelemetry.Extras.builder(), ActivityStreamContextMenu.MenuMode.HIGHLIGHT, item, null, null, 100, 100);

        final int expectedBookmarkString;
        if (bookmarked) {
            expectedBookmarkString = R.string.bookmark_remove;
        } else {
            expectedBookmarkString = R.string.bookmark;
        }

        final int expectedPinnedString;
        if (pinned) {
            expectedPinnedString = R.string.contextmenu_top_sites_unpin;
        } else {
            expectedPinnedString = R.string.contextmenu_top_sites_pin;
        }

        final MenuItem pinItem = menu.getItemByID(R.id.pin);
        assertMenuItemHasString(pinItem, expectedPinnedString);

        final MenuItem bookmarkItem = menu.getItemByID(R.id.bookmark);
        assertMenuItemHasString(bookmarkItem, expectedBookmarkString);

        final MenuItem deleteItem = menu.getItemByID(R.id.delete);
        assertMenuItemIsVisible(deleteItem, visited);

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


    private Highlight findHighlightByUrl(String url) {
        final Cursor cursor = db.getHighlights(getActivity(), 20).loadInBackground();
        mAsserter.isnot(cursor, null, "Highlights cursor is not null");

        try {
            mAsserter.ok(cursor.getCount() > 0, "Highlights cursor has entries", null);

            mAsserter.dumpLog("Database returned " + cursor.getCount() + " highlights");

            mAsserter.ok(cursor.moveToFirst(), "Move to beginning of cursor", null);

            do {
                final Highlight highlight = Highlight.fromCursor(cursor);
                if (url.equals(highlight.getUrl())) {
                    return highlight;
                }
            } while (cursor.moveToNext());

            mAsserter.ok(false, "Could not find highlight for URL: " + url, null);
            return null; // The call to fail() will throw an exception but the compiler doesn't know
        } finally {
            cursor.close();
        }
    }

    private TopSite findTopSiteByUrl(String url) {
        final Cursor cursor = db.getActivityStreamTopSites(getActivity(), 5, 20).loadInBackground();
        mAsserter.isnot(cursor, null, "Top Sites cursor is not null");

        try {
            mAsserter.ok(cursor.getCount() > 0, "Top Sites cursor has entries", null);

            mAsserter.ok(cursor.moveToFirst(), "Move to beginning of cursor", null);

            do {
                final TopSite topSite = TopSite.fromCursor(cursor);
                if (url.equals(topSite.getUrl())) {
                    return topSite;
                }
            } while (cursor.moveToNext());

            mAsserter.ok(false, "Could not find Top Site for URL: " + url, null);
            return null; // The call to fail() will throw an exception but the compiler doesn't know
        } finally {
            cursor.close();
        }
    }

    private void insertVisit(String url, long dateLastVisited) {
        ContentValues values = new ContentValues();
        values.put(BrowserContract.History.URL, url);
        values.put(BrowserContract.History.DATE_LAST_VISITED, dateLastVisited);
        values.put(BrowserContract.History.IS_DELETED, 0);

        Uri mHistoryUriWithProfile = DBUtils.appendProfile(
                GeckoProfile.get(getActivity()).getName(), BrowserContract.History.CONTENT_URI);

        Uri mUpdateHistoryUriWithProfile =
                mHistoryUriWithProfile.buildUpon()
                        .appendQueryParameter(BrowserContract.PARAM_INCREMENT_VISITS, "true")
                        .appendQueryParameter(BrowserContract.PARAM_INSERT_IF_NEEDED, "true")
                        .build();

        contentResolver.update(mUpdateHistoryUriWithProfile,
                values,
                BrowserContract.History.URL + " = ?",
                new String[] { url });
    }

    private void cleanup() {
        final Context context = getActivity();

        contentResolver = context.getContentResolver();

        // Let's always start with no traces in the db
        db = BrowserDB.from(context);
        db.removeHistoryEntry(contentResolver, TEST_URL);
        db.removeBookmarksWithURL(contentResolver, TEST_URL);
        db.unpinSiteForAS(contentResolver, TEST_URL);
    }
}

