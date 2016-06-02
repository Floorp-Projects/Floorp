/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.db;

import android.content.ContentProviderClient;
import android.content.ContentValues;
import android.database.Cursor;
import android.net.Uri;
import android.os.RemoteException;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.background.testhelpers.TestRunner;
import org.robolectric.shadows.ShadowContentResolver;

import static org.junit.Assert.*;

/**
 * Testing functionality exposed by BrowserProvider ContentProvider (history, bookmarks, etc).
 * This is WIP junit4 port of robocop tests at org.mozilla.gecko.tests.testBrowserProvider.
 * See Bug 1269492
 */
@RunWith(TestRunner.class)
public class BrowserProviderHistoryTest extends BrowserProviderHistoryVisitsTestBase {
    private ContentProviderClient thumbnailClient;
    private Uri thumbnailTestUri;
    private Uri expireHistoryNormalUri;
    private Uri expireHistoryAggressiveUri;

    private static final long THREE_MONTHS = 1000L * 60L * 60L * 24L * 30L * 3L;

    @Before
    @Override
    public void setUp() throws Exception {
        super.setUp();
        final ShadowContentResolver cr = new ShadowContentResolver();
        thumbnailClient = cr.acquireContentProviderClient(BrowserContract.Thumbnails.CONTENT_URI);
        thumbnailTestUri = testUri(BrowserContract.Thumbnails.CONTENT_URI);
        expireHistoryNormalUri = testUri(BrowserContract.History.CONTENT_OLD_URI).buildUpon()
                .appendQueryParameter(
                        BrowserContract.PARAM_EXPIRE_PRIORITY,
                        BrowserContract.ExpirePriority.NORMAL.toString()
                ).build();
        expireHistoryAggressiveUri = testUri(BrowserContract.History.CONTENT_OLD_URI).buildUpon()
                .appendQueryParameter(
                        BrowserContract.PARAM_EXPIRE_PRIORITY,
                        BrowserContract.ExpirePriority.AGGRESSIVE.toString()
                ).build();
    }

    /**
     * Test aggressive expiration on new (recent) history items
     */
    @Test
    public void testHistoryExpirationAggressiveNew() throws Exception {
        final int historyItemsCount = 3000;
        insertHistory(historyItemsCount, System.currentTimeMillis());

        historyClient.delete(expireHistoryAggressiveUri, null, null);

        /**
         * Aggressive expiration should leave 500 history items
         * See {@link BrowserProvider.AGGRESSIVE_EXPIRY_RETAIN_COUNT}
         */
        assertRowCount(historyClient, historyTestUri, 500);

        /**
         * Aggressive expiration should leave 15 thumbnails
         * See {@link BrowserProvider.DEFAULT_EXPIRY_THUMBNAIL_COUNT}
         */
        assertRowCount(thumbnailClient, thumbnailTestUri, 15);
    }

    /**
     * Test normal expiration on new (recent) history items
     */
    @Test
    public void testHistoryExpirationNormalNew() throws Exception {
        final int historyItemsCount = 3000;
        insertHistory(historyItemsCount, System.currentTimeMillis());

        historyClient.delete(expireHistoryNormalUri, null, null);

        // Normal expiration shouldn't expire new items
        assertRowCount(historyClient, historyTestUri, 3000);

        /**
         * Normal expiration should leave 15 thumbnails
         * See {@link BrowserProvider.DEFAULT_EXPIRY_THUMBNAIL_COUNT}
         */
        assertRowCount(thumbnailClient, thumbnailTestUri, 15);
    }

    /**
     * Test aggressive expiration on old history items
     */
    @Test
    public void testHistoryExpirationAggressiveOld() throws Exception {
        final int historyItemsCount = 3000;
        insertHistory(historyItemsCount, System.currentTimeMillis() - THREE_MONTHS);

        historyClient.delete(expireHistoryAggressiveUri, null, null);

        /**
         * Aggressive expiration should leave 500 history items
         * See {@link BrowserProvider.AGGRESSIVE_EXPIRY_RETAIN_COUNT}
         */
        assertRowCount(historyClient, historyTestUri, 500);

        /**
         * Aggressive expiration should leave 15 thumbnails
         * See {@link BrowserProvider.DEFAULT_EXPIRY_THUMBNAIL_COUNT}
         */
        assertRowCount(thumbnailClient, thumbnailTestUri, 15);
    }

    /**
     * Test normal expiration on old history items
     */
    @Test
    public void testHistoryExpirationNormalOld() throws Exception {
        final int historyItemsCount = 3000;
        insertHistory(historyItemsCount, System.currentTimeMillis() - THREE_MONTHS);

        historyClient.delete(expireHistoryNormalUri, null, null);

        /**
         * Normal expiration of old items should retain at most 2000 items
         * See {@link BrowserProvider.DEFAULT_EXPIRY_RETAIN_COUNT}
         */
        assertRowCount(historyClient, historyTestUri, 2000);

        /**
         * Normal expiration should leave 15 thumbnails
         * See {@link BrowserProvider.DEFAULT_EXPIRY_THUMBNAIL_COUNT}
         */
        assertRowCount(thumbnailClient, thumbnailTestUri, 15);
    }

    /**
     * Test that we update aggregates at the appropriate times. Local visit aggregates are only updated
     * when updating history record with PARAM_INCREMENT_VISITS=true. Remote aggregate values are updated
     * only if set directly. Aggregate values are not set when inserting a new history record via insertHistory.
     * Local aggregate values are set when inserting a new history record via update.
     * @throws Exception
     */
    @Test
    public void testHistoryVisitAggregates() throws Exception {
        final long baseDate = System.currentTimeMillis();
        final String url = "https://www.mozilla.org";
        final Uri historyIncrementVisitsUri = historyTestUri.buildUpon()
                .appendQueryParameter(BrowserContract.PARAM_INCREMENT_VISITS, "true")
                .appendQueryParameter(BrowserContract.PARAM_INSERT_IF_NEEDED, "true").build();

        // Test default values
        insertHistoryItem(url, null, baseDate, null);
        assertHistoryAggregates(BrowserContract.History.URL + " = ?", new String[] {url},
                0, 0, 0, 0, 0);

        // Test setting visit count on new history item creation
        final String url2 = "https://www.eff.org";
        insertHistoryItem(url2, null, baseDate, 17);
        assertHistoryAggregates(BrowserContract.History.URL + " = ?", new String[] {url2},
                17, 0, 0, 0, 0);

        // Test setting visit count on new history item creation via .update
        final String url3 = "https://www.torproject.org";
        final ContentValues cv = new ContentValues();
        cv.put(BrowserContract.History.URL, url3);
        cv.put(BrowserContract.History.VISITS, 13);
        cv.put(BrowserContract.History.DATE_LAST_VISITED, baseDate);
        historyClient.update(historyIncrementVisitsUri, cv, BrowserContract.History.URL + " = ?", new String[] {url3});
        assertHistoryAggregates(BrowserContract.History.URL + " = ?", new String[] {url3},
                13, 13, baseDate, 0, 0);

        // Test that updating meta doesn't touch aggregates
        cv.clear();
        cv.put(BrowserContract.History.TITLE, "New title");
        historyClient.update(historyTestUri, cv, BrowserContract.History.URL + " = ?", new String[] {url});
        assertHistoryAggregates(BrowserContract.History.URL + " = ?", new String[] {url},
                0, 0, 0, 0, 0);

        // Test that incrementing visits without specifying visit count updates local aggregate values
        final long lastVisited = System.currentTimeMillis();
        cv.clear();
        cv.put(BrowserContract.History.DATE_LAST_VISITED, lastVisited);
        historyClient.update(historyIncrementVisitsUri,
                cv, BrowserContract.History.URL + " = ?", new String[] {url});
        assertHistoryAggregates(BrowserContract.History.URL + " = ?", new String[] {url},
                1, 1, lastVisited, 0, 0);

        // Test that incrementing visits by a specified visit count updates local aggregate values
        // We don't support bumping visit count by more than 1. This doesn't make sense when we keep
        // detailed information about our individual visits.
        final long lastVisited2 = System.currentTimeMillis();
        cv.clear();
        cv.put(BrowserContract.History.DATE_LAST_VISITED, lastVisited2);
        cv.put(BrowserContract.History.VISITS, 10);
        historyClient.update(
                historyTestUri.buildUpon()
                        .appendQueryParameter(BrowserContract.PARAM_INCREMENT_VISITS, "true").build(),
                cv, BrowserContract.History.URL + " = ?", new String[] {url});
        assertHistoryAggregates(BrowserContract.History.URL + " = ?", new String[] {url},
                2, 2, lastVisited2, 0, 0);

        // Test that we can directly update aggregate values
        // NB: visits is unchanged (2)
        final long lastVisited3 = System.currentTimeMillis();
        cv.clear();
        cv.put(BrowserContract.History.LOCAL_DATE_LAST_VISITED, lastVisited3);
        cv.put(BrowserContract.History.LOCAL_VISITS, 19);
        cv.put(BrowserContract.History.REMOTE_DATE_LAST_VISITED, lastVisited3 - 100);
        cv.put(BrowserContract.History.REMOTE_VISITS, 3);
        historyClient.update(historyTestUri, cv, BrowserContract.History.URL + " = ?", new String[] {url});
        assertHistoryAggregates(BrowserContract.History.URL + " = ?", new String[] {url},
                2, 19, lastVisited3, 3, lastVisited3 - 100);

        // Test that we can set remote aggregate count to a specific value
        cv.clear();
        cv.put(BrowserContract.History.REMOTE_VISITS, 5);
        historyClient.update(historyTestUri, cv, BrowserContract.History.URL + " = ?", new String[] {url});
        assertHistoryAggregates(BrowserContract.History.URL + " = ?", new String[] {url},
                2, 19, lastVisited3, 5, lastVisited3 - 100);

        // Test that we can increment remote aggregate value by setting a query param in the URI
        final Uri historyIncrementRemoteAggregateUri = historyTestUri.buildUpon()
                .appendQueryParameter(BrowserContract.PARAM_INCREMENT_REMOTE_AGGREGATES, "true")
                .build();
        cv.clear();
        cv.put(BrowserContract.History.REMOTE_DATE_LAST_VISITED, lastVisited3);
        cv.put(BrowserContract.History.REMOTE_VISITS, 3);
        historyClient.update(historyIncrementRemoteAggregateUri, cv, BrowserContract.History.URL + " = ?", new String[] {url});
        // NB: remoteVisits=8. Previous value was 5, and we're incrementing by 3.
        assertHistoryAggregates(BrowserContract.History.URL + " = ?", new String[] {url},
                2, 19, lastVisited3, 8, lastVisited3);

        // Test that we throw when trying to increment REMOTE_VISITS without passing in "increment by" value
        cv.clear();
        try {
            historyClient.update(historyIncrementRemoteAggregateUri, cv, BrowserContract.History.URL + " = ?", new String[]{url});
            assertTrue("Expected to throw IllegalArgumentException", false);
        } catch (IllegalArgumentException e) {
            assertTrue(true);

            // NB: same values as above, to ensure throwing update didn't actually change anything.
            assertHistoryAggregates(BrowserContract.History.URL + " = ?", new String[] {url},
                    2, 19, lastVisited3, 8, lastVisited3);
        }
    }

    private void assertHistoryAggregates(String selection, String[] selectionArg, int visits, int localVisits, long localLastVisited, int remoteVisits, long remoteLastVisited) throws Exception {
        final Cursor c = historyClient.query(historyTestUri, new String[] {
                BrowserContract.History.VISITS,
                BrowserContract.History.LOCAL_VISITS,
                BrowserContract.History.REMOTE_VISITS,
                BrowserContract.History.LOCAL_DATE_LAST_VISITED,
                BrowserContract.History.REMOTE_DATE_LAST_VISITED
        }, selection, selectionArg, null);

        assertNotNull(c);
        try {
            assertTrue(c.moveToFirst());

            final int visitsCol = c.getColumnIndexOrThrow(BrowserContract.History.VISITS);
            final int localVisitsCol = c.getColumnIndexOrThrow(BrowserContract.History.LOCAL_VISITS);
            final int remoteVisitsCol = c.getColumnIndexOrThrow(BrowserContract.History.REMOTE_VISITS);
            final int localDateLastVisitedCol = c.getColumnIndexOrThrow(BrowserContract.History.LOCAL_DATE_LAST_VISITED);
            final int remoteDateLastVisitedCol = c.getColumnIndexOrThrow(BrowserContract.History.REMOTE_DATE_LAST_VISITED);

            assertEquals(visits, c.getInt(visitsCol));

            assertEquals(localVisits, c.getInt(localVisitsCol));
            assertEquals(localLastVisited, c.getLong(localDateLastVisitedCol));

            assertEquals(remoteVisits, c.getInt(remoteVisitsCol));
            assertEquals(remoteLastVisited, c.getLong(remoteDateLastVisitedCol));
        } finally {
            c.close();
        }
    }

    /**
     * Insert <code>count</code> history records with thumbnails, and for a third of records insert a visit.
     * Inserting visits only for some of the history records is in order to ensure we're correctly JOIN-ing
     * History and Visits tables in the Combined view.
     * Will ensure that date_created and date_modified for new records are the same as last visited date.
     *
     * @param count number of history records to insert
     * @param baseTime timestamp which will be used as a basis for last visited date
     * @throws RemoteException
     */
    private void insertHistory(int count, long baseTime) throws RemoteException {
        Uri incrementUri = historyTestUri.buildUpon()
                .appendQueryParameter(BrowserContract.PARAM_INCREMENT_VISITS, "true").build();

        for (int i = 0; i < count; i++) {
            final String url = "https://www.mozilla" + i + ".org";
            insertHistoryItem(url, "testGUID" + i, baseTime - i, null);
            if (i % 3 == 0) {
                assertEquals(1, historyClient.update(incrementUri, new ContentValues(), BrowserContract.History.URL + " = ?", new String[]{url}));
            }

            // inserting a new entry sets the date created and modified automatically, so let's reset them
            ContentValues cv = new ContentValues();
            cv.put(BrowserContract.History.DATE_CREATED, baseTime - i);
            cv.put(BrowserContract.History.DATE_MODIFIED, baseTime - i);
            assertEquals(1, historyClient.update(historyTestUri, cv, BrowserContract.History.URL + " = ?",
                    new String[] { "https://www.mozilla" + i + ".org" }));
        }

        // insert thumbnails for history items
        ContentValues[] thumbs = new ContentValues[count];
        for (int i = 0; i < count; i++) {
            thumbs[i] = new ContentValues();
            thumbs[i].put(BrowserContract.Thumbnails.DATA, i);
            thumbs[i].put(BrowserContract.Thumbnails.URL, "https://www.mozilla" + i + ".org");
        }
        assertEquals(count, thumbnailClient.bulkInsert(thumbnailTestUri, thumbs));
    }

    private void assertRowCount(final ContentProviderClient client, final Uri uri, final int count) throws RemoteException {
        final Cursor c = client.query(uri, null, null, null, null);
        assertNotNull(c);
        try {
            assertEquals(count, c.getCount());
        } finally {
            c.close();
        }
    }
}