/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.db;

import android.content.ContentProviderClient;
import android.content.ContentValues;
import android.database.Cursor;
import android.net.Uri;
import android.os.RemoteException;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.background.testhelpers.TestRunner;
import org.mozilla.gecko.sync.repositories.android.BrowserContractHelpers;
import org.mozilla.gecko.sync.setup.Constants;

import java.util.UUID;
import java.util.concurrent.TimeUnit;

import static org.mozilla.gecko.db.BrowserContract.PARAM_PROFILE;

/**
 * Unit tests for the highlights query (Activity Stream).
 */
@RunWith(TestRunner.class)
public class BrowserProviderHighlightsTest extends BrowserProviderHistoryVisitsTestBase {
    private ContentProviderClient highlightsClient;
    private ContentProviderClient activityStreamBlocklistClient;
    private ContentProviderClient bookmarksClient;

    private Uri highlightsTestUri;
    private Uri activityStreamBlocklistTestUri;
    private Uri bookmarksTestUri;

    private Uri expireHistoryNormalUri;

    @Before
    public void setUp() throws Exception {
        super.setUp();

        final Uri highlightsClientUri = BrowserContract.Highlights.CONTENT_URI.buildUpon()
                .appendQueryParameter(PARAM_PROFILE, Constants.DEFAULT_PROFILE)
                .build();

        final Uri activityStreamBlocklistClientUri = BrowserContract.ActivityStreamBlocklist.CONTENT_URI.buildUpon()
                .appendQueryParameter(PARAM_PROFILE, Constants.DEFAULT_PROFILE)
                .build();

        highlightsClient = contentResolver.acquireContentProviderClient(highlightsClientUri);
        activityStreamBlocklistClient = contentResolver.acquireContentProviderClient(activityStreamBlocklistClientUri);
        bookmarksClient = contentResolver.acquireContentProviderClient(BrowserContractHelpers.BOOKMARKS_CONTENT_URI);

        highlightsTestUri = testUri(BrowserContract.Highlights.CONTENT_URI);
        activityStreamBlocklistTestUri = testUri(BrowserContract.ActivityStreamBlocklist.CONTENT_URI);
        bookmarksTestUri = testUri(BrowserContract.Bookmarks.CONTENT_URI);

        expireHistoryNormalUri = testUri(BrowserContract.History.CONTENT_OLD_URI).buildUpon()
                .appendQueryParameter(
                        BrowserContract.PARAM_EXPIRE_PRIORITY,
                        BrowserContract.ExpirePriority.NORMAL.toString()
                ).build();
    }

    @After
    public void tearDown() {
        highlightsClient.release();
        activityStreamBlocklistClient.release();
        bookmarksClient.release();

        super.tearDown();
    }

    /**
     * Scenario: Empty database, no history, no bookmarks.
     *
     * Assert that:
     *  - Empty cursor (not null) is returned.
     */
    @Test
    public void testEmptyDatabase() throws Exception {
        final Cursor cursor = highlightsClient.query(highlightsTestUri, null, null, null, null);
        Assert.assertNotNull(cursor);

        Assert.assertEquals(0, cursor.getCount());

        cursor.close();
    }

    /**
     * Scenario: The database only contains very recent history (now, 5 minutes ago, 20 minutes).
     *
     * Assert that:
     *  - No highlight is returned from recent history.
     */
    @Test
    public void testOnlyRecentHistory() throws Exception {
        final long now = System.currentTimeMillis();
        final long fiveMinutesAgo = now - 1000 * 60 * 5;
        final long twentyMinutes = now - 1000 * 60 * 20;

        insertHistoryItem(createUniqueUrl(), createGUID(), now, 1, createUniqueTitle());
        insertHistoryItem(createUniqueUrl(), createGUID(), fiveMinutesAgo, 1, createUniqueTitle());
        insertHistoryItem(createUniqueUrl(), createGUID(), twentyMinutes, 1, createUniqueTitle());

        final Cursor cursor = highlightsClient.query(highlightsTestUri, null, null, null, null);

        Assert.assertNotNull(cursor);

        Assert.assertEquals(0, cursor.getCount());

        cursor.close();
    }

    /**
     * Scenario: The database contains recent (but not too fresh) history (1 hour, 5 days).
     *
     * Assert that:
     *  - Highlights are returned from history.
     */
    @Test
    public void testHighlightsArePickedFromHistory() throws Exception {
        final String url1 = createUniqueUrl();
        final String url2 = createUniqueUrl();
        final String title1 = createUniqueTitle();
        final String title2 = createUniqueTitle();

        final long oneHourAgo = System.currentTimeMillis() - 1000 * 60 * 60;
        final long fiveDaysAgo = System.currentTimeMillis() - 1000 * 60 * 60 * 24 * 5;

        insertHistoryItem(url1, createGUID(), oneHourAgo, 1, title1);
        insertHistoryItem(url2, createGUID(), fiveDaysAgo, 1, title2);

        final Cursor cursor = highlightsClient.query(highlightsTestUri, null, null, null, null);
        Assert.assertNotNull(cursor);

        Assert.assertEquals(2, cursor.getCount());

        assertCursorContainsEntry(cursor, url1, title1);
        assertCursorContainsEntry(cursor, url2, title2);

        cursor.close();
    }

    /**
     * Scenario: The database contains history that is visited frequently and rarely.
     *
     * Assert that:
     *  - Highlights are picked from rarely visited websites.
     *  - Highlights are not picked from frequently visited websites.
     */
    @Test
    public void testOftenVisitedPagesAreNotPicked() throws Exception {
        final String url1 = createUniqueUrl();
        final String title1 = createUniqueTitle();

        final long oneHourAgo = System.currentTimeMillis() - 1000 * 60 * 60;
        final long fiveDaysAgo = System.currentTimeMillis() - 1000 * 60 * 60 * 24 * 5;

        insertHistoryItem(url1, createGUID(), oneHourAgo, 2, title1);
        insertHistoryItem(createUniqueUrl(), createGUID(), fiveDaysAgo, 25, createUniqueTitle());

        final Cursor cursor = highlightsClient.query(highlightsTestUri, null, null, null, null);
        Assert.assertNotNull(cursor);

        // Verify that only the first URL (with one visit) is picked and the second URL with 25 visits is ignored.

        Assert.assertEquals(1, cursor.getCount());

        cursor.moveToNext();
        assertCursor(cursor, url1, title1);

        cursor.close();
    }

    /**
     * Scenario: The database contains history with and without titles.
     *
     * Assert that:
     * - History without titles is not picked for highlights.
     */
    @Test
    public void testHistoryWithoutTitlesIsNotPicked() throws Exception {
        final String url1 = createUniqueUrl();
        final String url2 = createUniqueUrl();
        final String title1 = "";
        final String title2 = createUniqueTitle();

        final long oneHourAgo = System.currentTimeMillis() - 1000 * 60 * 60;
        final long fiveDaysAgo = System.currentTimeMillis() - 1000 * 60 * 60 * 24 * 5;

        insertHistoryItem(url1, createGUID(), oneHourAgo, 1, title1);
        insertHistoryItem(url2, createGUID(), fiveDaysAgo, 1, title2);

        final Cursor cursor = highlightsClient.query(highlightsTestUri, null, null, null, null);
        Assert.assertNotNull(cursor);

        // Only one bookmark will be picked for highlights
        Assert.assertEquals(1, cursor.getCount());

        cursor.moveToNext();
        assertCursor(cursor, url2, title2);

        cursor.close();
    }

    /**
     * Scenario: Database contains two bookmarks (unvisited).
     *
     * Assert that:
     *  - One bookmark is picked for highlights.
     */
    @Test
    public void testPickingBookmarkForHighlights() throws Exception {
        final long oneHourAgo = System.currentTimeMillis() - 1000 * 60 * 60;
        final long fiveDaysAgo = System.currentTimeMillis() - 1000 * 60 * 60 * 24 * 5;

        final String url1 = createUniqueUrl();
        final String url2 = createUniqueUrl();
        final String title1 = createUniqueTitle();
        final String title2 = createUniqueTitle();

        insertBookmarkItem(url1, title1, oneHourAgo);
        insertBookmarkItem(url2, title2, fiveDaysAgo);

        final Cursor cursor = highlightsClient.query(highlightsTestUri, null, null, null, null);
        Assert.assertNotNull(cursor);

        Assert.assertEquals(1, cursor.getCount());

        cursor.moveToNext();
        assertCursor(cursor, url1, title1);

        cursor.close();
    }

    /**
     * Scenario: Database contains an often visited bookmark.
     *
     * Assert that:
     *  - Bookmark is not selected for highlights.
     */
    @Test
    public void testOftenVisitedBookmarksWillNotBePicked() throws Exception {
        final String url = createUniqueUrl();
        final long oneHourAgo = System.currentTimeMillis() - 1000 * 60 * 60;

        insertBookmarkItem(url, createUniqueTitle(), oneHourAgo);
        insertHistoryItem(url, createGUID(), oneHourAgo, 25, createUniqueTitle());

        final Cursor cursor = highlightsClient.query(highlightsTestUri, null, null, null, null);
        Assert.assertNotNull(cursor);

        Assert.assertEquals(0, cursor.getCount());

        cursor.close();
    }

    /**
     * Scenario: Database contains URL as bookmark and in history (not visited often).
     *
     * Assert that:
     * - URL is not picked twice (as bookmark and from history)
     */
    @Test
    public void testSameUrlIsNotPickedFromHistoryAndBookmarks() throws Exception {
        final String url = createUniqueUrl();

        final long oneHourAgo = System.currentTimeMillis() - 1000 * 60 * 60;

        // Insert bookmark that is picked for highlights
        insertBookmarkItem(url, createUniqueTitle(), oneHourAgo);
        // Insert history for same URL that would be picked for highlights too
        insertHistoryItem(url, createGUID(), oneHourAgo, 2, createUniqueTitle());

        final Cursor cursor = highlightsClient.query(highlightsTestUri, null, null, null, null);
        Assert.assertNotNull(cursor);

        Assert.assertEquals(1, cursor.getCount());

        cursor.close();
    }

    /**
     * Scenario: Database contains only old bookmarks.
     *
     * Assert that:
     * - Old bookmarks are not selected as highlight.
     */
    @Test
    public void testVeryOldBookmarksAreNotSelected() throws Exception {
        final long oneWeekAgo = System.currentTimeMillis() - TimeUnit.DAYS.toMillis(7);
        final long oneMonthAgo = System.currentTimeMillis() - TimeUnit.DAYS.toMillis(31);
        final long oneYearAgo = System.currentTimeMillis() - TimeUnit.DAYS.toMillis(365);

        insertBookmarkItem(createUniqueUrl(), createUniqueTitle(), oneWeekAgo);
        insertBookmarkItem(createUniqueUrl(), createUniqueTitle(), oneMonthAgo);
        insertBookmarkItem(createUniqueUrl(), createUniqueTitle(), oneYearAgo);

        final Cursor cursor = highlightsClient.query(highlightsTestUri, null, null, null, null);
        Assert.assertNotNull(cursor);

        Assert.assertEquals(0, cursor.getCount());

        cursor.close();
    }

    @Test
    public void testBlocklistItemsAreNotSelected() throws Exception {
        final long oneDayAgo = System.currentTimeMillis() - TimeUnit.DAYS.toMillis(1);

        final String blockURL = createUniqueUrl();

        insertBookmarkItem(blockURL, createUniqueTitle(), oneDayAgo);

        Cursor cursor = highlightsClient.query(highlightsTestUri, null, null, null, null);
        Assert.assertNotNull(cursor);
        Assert.assertEquals(1, cursor.getCount());
        cursor.close();

        insertBlocklistItem(blockURL);

        cursor = highlightsClient.query(highlightsTestUri, null, null, null, null);
        Assert.assertNotNull(cursor);
        Assert.assertEquals(0, cursor.getCount());
        cursor.close();
    }

    @Test
    public void testBlocklistItemsExpire() throws Exception {
        final long oneDayAgo = System.currentTimeMillis() - TimeUnit.DAYS.toMillis(1);

        final String blockURL = createUniqueUrl();
        final String blockTitle = createUniqueTitle();

        insertBookmarkItem(blockURL, blockTitle, oneDayAgo);
        insertBlocklistItem(blockURL);

        {
            final Cursor cursor = highlightsClient.query(highlightsTestUri, null, null, null, null);
            Assert.assertNotNull(cursor);
            Assert.assertEquals(0, cursor.getCount());
            cursor.close();
        }

        // Add (2000 / 10) items in the loop -> 201 items total
        int itemsNeeded = BrowserProvider.DEFAULT_EXPIRY_RETAIN_COUNT / BrowserProvider.ACTIVITYSTREAM_BLOCKLIST_EXPIRY_FACTOR;
        for (int i = 0; i < itemsNeeded; i++) {
            insertBlocklistItem(createUniqueUrl());
        }

        // We still have zero highlights: the item is still blocked
        {
            final Cursor cursor = highlightsClient.query(highlightsTestUri, null, null, null, null);
            Assert.assertNotNull(cursor);
            Assert.assertEquals(0, cursor.getCount());
            cursor.close();
        }

        // expire the original blocked URL - only most recent 200 items are retained
        historyClient.delete(expireHistoryNormalUri, null, null);

        // And the original URL is now in highlights again (note: this shouldn't happen in real life,
        // since the URL will no longer be eligible for highlights by the time we expire it)
        {
            final Cursor cursor = highlightsClient.query(highlightsTestUri, null, null, null, null);
            Assert.assertNotNull(cursor);
            Assert.assertEquals(1, cursor.getCount());

            cursor.moveToFirst();
            assertCursor(cursor, blockURL, blockTitle);
            cursor.close();
        }
    }

    private void insertBookmarkItem(String url, String title, long createdAt) throws RemoteException {
        ContentValues values = new ContentValues();

        values.put(BrowserContract.Bookmarks.URL, url);
        values.put(BrowserContract.Bookmarks.TITLE, title);
        values.put(BrowserContract.Bookmarks.PARENT, 0);
        values.put(BrowserContract.Bookmarks.TYPE, BrowserContract.Bookmarks.TYPE_BOOKMARK);
        values.put(BrowserContract.Bookmarks.DATE_CREATED, createdAt);

        bookmarksClient.insert(bookmarksTestUri, values);
    }

    private void insertBlocklistItem(String url) throws RemoteException {
        final ContentValues values = new ContentValues();
        values.put(BrowserContract.ActivityStreamBlocklist.URL, url);

        activityStreamBlocklistClient.insert(activityStreamBlocklistTestUri, values);
    }

    private void assertCursor(Cursor cursor, String url, String title) {
        final String actualTitle = cursor.getString(cursor.getColumnIndexOrThrow(BrowserContract.Combined.TITLE));
        Assert.assertEquals(title, actualTitle);

        final String actualUrl = cursor.getString(cursor.getColumnIndexOrThrow(BrowserContract.Combined.URL));
        Assert.assertEquals(url, actualUrl);
    }

    private void assertCursorContainsEntry(Cursor cursor, String url, String title) {
        cursor.moveToFirst();

        do {
            final String actualTitle = cursor.getString(cursor.getColumnIndexOrThrow(BrowserContract.Combined.TITLE));
            final String actualUrl = cursor.getString(cursor.getColumnIndexOrThrow(BrowserContract.Combined.URL));

            if (actualTitle.equals(title) && actualUrl.equals(url)) {
                return;
            }
        } while (cursor.moveToNext());

        Assert.fail("Could not find entry title=" + title + ", url=" + url);
    }

    private String createUniqueUrl() {
        return new Uri.Builder()
                .scheme("https")
                .authority(UUID.randomUUID().toString() + ".example.org")
                .appendPath(UUID.randomUUID().toString())
                .appendPath(UUID.randomUUID().toString())
                .build()
                .toString();
    }

    private String createUniqueTitle() {
        return "Title " + UUID.randomUUID().toString();
    }

    private String createGUID() {
        return UUID.randomUUID().toString();
    }
}
