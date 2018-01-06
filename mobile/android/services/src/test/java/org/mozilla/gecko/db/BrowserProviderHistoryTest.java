/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.db;

import android.content.ContentProviderClient;
import android.content.ContentValues;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.os.RemoteException;

import org.junit.After;
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

    @After
    @Override
    public void tearDown() {
        thumbnailClient.release();
        super.tearDown();
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

    private void assertHistoryValuesForGuidsFromSync(int expectedCount, String title, String url, Long remoteLastVisited, Integer visits) throws RemoteException {
        final Cursor c = historyClient.query(historyTestUri, new String[] {
                BrowserContract.History.TITLE,
                BrowserContract.History.VISITS,
                BrowserContract.History.URL,
                BrowserContract.History.LOCAL_VISITS,
                BrowserContract.History.REMOTE_VISITS,
                BrowserContract.History.LOCAL_DATE_LAST_VISITED,
                BrowserContract.History.REMOTE_DATE_LAST_VISITED,
                BrowserContract.History.DATE_CREATED,
                BrowserContract.History.DATE_MODIFIED
        }, null, null, BrowserContract.History._ID + " DESC");

        // 3m ago, since one of the tests manually rewinds timestamps to 2 months ago.
        final long reasonablyRecentTimestamp = System.currentTimeMillis() - 1000L * 60L * 60L * 24L * 30L * 3L;

        assertNotNull(c);
        assertEquals(expectedCount, c.getCount());
        try {
            final int titleCol = c.getColumnIndexOrThrow(BrowserContract.History.TITLE);
            final int urlCol = c.getColumnIndexOrThrow(BrowserContract.History.URL);
            final int visitsCol = c.getColumnIndexOrThrow(BrowserContract.History.VISITS);
            final int localVisitsCol = c.getColumnIndexOrThrow(BrowserContract.History.LOCAL_VISITS);
            final int remoteVisitsCol = c.getColumnIndexOrThrow(BrowserContract.History.REMOTE_VISITS);
            final int localDateLastVisitedCol = c.getColumnIndexOrThrow(BrowserContract.History.LOCAL_DATE_LAST_VISITED);
            final int remoteDateLastVisitedCol = c.getColumnIndexOrThrow(BrowserContract.History.REMOTE_DATE_LAST_VISITED);
            final int dateCreatedCol = c.getColumnIndexOrThrow(BrowserContract.History.DATE_CREATED);
            final int dateModifiedCol = c.getColumnIndexOrThrow(BrowserContract.History.DATE_MODIFIED);

            while (c.moveToNext()) {
                assertEquals(title, c.getString(titleCol));
                assertEquals(url, c.getString(urlCol));

                // History is inserted 'from sync', so expect local visit counts and local visit dates
                // to be null.
                assertEquals(0, c.getInt(localVisitsCol));
                assertEquals(0, c.getLong(localDateLastVisitedCol));

                if (remoteLastVisited == null) {
                    assertEquals(0, c.getInt(remoteDateLastVisitedCol));
                } else {
                    assertEquals(remoteLastVisited, (Long) c.getLong(remoteDateLastVisitedCol));
                    assertEquals(visits, (Integer) c.getInt(remoteVisitsCol));
                    assertEquals(visits, (Integer) c.getInt(visitsCol));
                }

                // Make sure 'modified' and 'created' timestamps are present.
                assertFalse(c.isNull(dateCreatedCol));
                assertFalse(c.isNull(dateModifiedCol));

                // Make sure these timestamps are also reasonable. Hopefully this doesn't fail due
                // to poorly timed clock drift.
                final long createdTimestamp = c.getLong(dateCreatedCol);
                final long modifiedTimestamp = c.getLong(dateModifiedCol);
                assertTrue(createdTimestamp + " must be greater than " + reasonablyRecentTimestamp,
                        reasonablyRecentTimestamp < createdTimestamp);
                assertTrue(modifiedTimestamp + " must be greater than " + reasonablyRecentTimestamp,
                        reasonablyRecentTimestamp < c.getLong(dateModifiedCol));
            }
        } finally {
            c.close();
        }
    }

    @Test
    public void testBulkHistoryInsert() throws Exception {
        Bundle result;

        // Test basic error conditions.
        String historyTestUriArg = historyTestUri.toString();
        try {
            historyClient.call(BrowserContract.METHOD_INSERT_HISTORY_WITH_VISITS_FROM_SYNC, historyTestUriArg, new Bundle());
            fail();
        } catch (IllegalArgumentException e) {}

        final Bundle data = new Bundle();

        Bundle[] recordBundles = new Bundle[0];
        data.putSerializable(BrowserContract.METHOD_PARAM_DATA, recordBundles);
        result = historyClient.call(BrowserContract.METHOD_INSERT_HISTORY_WITH_VISITS_FROM_SYNC, historyTestUriArg, data);
        assertNotNull(result);
        assertNull(result.getSerializable(BrowserContract.METHOD_RESULT));
        assertRowCount(historyClient, historyTestUri, 0);

        // Test insert three history records with 10 visits each.
        recordBundles = new Bundle[3];
        for (int i = 0; i < 3; i++) {
            final Bundle bundle = new Bundle();
            bundle.putParcelable(BrowserContract.METHOD_PARAM_OBJECT, buildHistoryCV("guid" + i, "Test", "https://www.mozilla.org/", 10L, 10L, 10));
            bundle.putSerializable(BrowserContract.History.VISITS, buildHistoryVisitsCVs(10, "guid" + i, 1L, 3, false));
            recordBundles[i] = bundle;
        }
        data.putSerializable(BrowserContract.METHOD_PARAM_DATA, recordBundles);

        result = historyClient.call(BrowserContract.METHOD_INSERT_HISTORY_WITH_VISITS_FROM_SYNC, historyTestUriArg, data);
        assertNotNull(result);
        assertNull(result.getSerializable(BrowserContract.METHOD_RESULT));
        assertRowCount(historyClient, historyTestUri, 3);
        assertRowCount(visitsClient, visitsTestUri, 30);

        // Ensure that bulk-inserted records look sane.
        assertHistoryValuesForGuidsFromSync(3, "Test", "https://www.mozilla.org/", 10L, 10);

        // Test insert mixed data.
        recordBundles = new Bundle[3];
        final Bundle bundle = new Bundle();
        bundle.putParcelable(BrowserContract.METHOD_PARAM_OBJECT, buildHistoryCV("guid4", null, "https://www.mozilla.org/1", null, null, null));
        bundle.putSerializable(BrowserContract.History.VISITS, new ContentValues[0]);
        recordBundles[0] = bundle;
        final Bundle bundle2 = new Bundle();
        bundle2.putParcelable(BrowserContract.METHOD_PARAM_OBJECT, buildHistoryCV("guid5", "Test", "https://www.mozilla.org/2", null, null, null));
        bundle2.putSerializable(BrowserContract.History.VISITS, new ContentValues[0]);
        recordBundles[1] = bundle2;
        final Bundle bundle3 = new Bundle();
        bundle3.putParcelable(BrowserContract.METHOD_PARAM_OBJECT, buildHistoryCV("guid6", "Test", "https://www.mozilla.org/3", 5L, 5L, 5));
        bundle3.putSerializable(BrowserContract.History.VISITS, buildHistoryVisitsCVs(5, "guid6", 1L, 2, false));
        recordBundles[2] = bundle3;
        data.putSerializable(BrowserContract.METHOD_PARAM_DATA, recordBundles);

        result = historyClient.call(BrowserContract.METHOD_INSERT_HISTORY_WITH_VISITS_FROM_SYNC, historyTestUriArg, data);
        assertNotNull(result);
        assertNull(result.getSerializable(BrowserContract.METHOD_RESULT));
        assertRowCount(historyClient, historyTestUri, 6);
        assertRowCount(visitsClient, visitsTestUri, 35);

        assertHistoryAggregates(BrowserContract.History.URL + " = ?", new String[] {"https://www.mozilla.org/3"},
                5, 0, 0, 5, 5);
    }

    /**
     * Tests that bulk-inserted history records without visits look correct.
     */
    @Test
    public void testBulkHistoryInsertWithoutVisits() throws Exception {
        final Bundle data = new Bundle();
        // Test insert three history records with 10 visits each.
        final int insertedRecordCount = 10;

        Bundle[] recordBundles = new Bundle[insertedRecordCount];
        for (int i = 0; i < insertedRecordCount; i++) {
            final Bundle bundle = new Bundle();
            bundle.putParcelable(BrowserContract.METHOD_PARAM_OBJECT, buildHistoryCV("guid" + i, "Test", "https://www.mozilla.org/", null, null, null));
            bundle.putSerializable(BrowserContract.History.VISITS, new ContentValues[0]);
            recordBundles[i] = bundle;
        }
        data.putSerializable(BrowserContract.METHOD_PARAM_DATA, recordBundles);

        Bundle result = historyClient.call(BrowserContract.METHOD_INSERT_HISTORY_WITH_VISITS_FROM_SYNC, historyTestUri.toString(), data);
        assertNotNull(result);
        assertNull(result.getSerializable(BrowserContract.METHOD_RESULT));
        assertRowCount(historyClient, historyTestUri, insertedRecordCount);
        assertRowCount(visitsClient, visitsTestUri, 0);

        assertHistoryValuesForGuidsFromSync(insertedRecordCount, "Test", "https://www.mozilla.org/", null, null);
    }

    /**
     * Tests bulk-inserting history records, resetting modified/created timestamps to be older than
     * the normal expiration 'keepAfter' threshold, and then running a normal expiration.
     */
    @Test
    public void testBullkHistoryInsertThenNormalExpire() throws Exception {
        final Bundle data = new Bundle();
        // Test insert three history records with 10 visits each.
        final int insertedRecordCount = 3000;

        Bundle[] recordBundles = new Bundle[insertedRecordCount];
        for (int i = 0; i < insertedRecordCount; i++) {
            final Bundle bundle = new Bundle();
            bundle.putParcelable(BrowserContract.METHOD_PARAM_OBJECT, buildHistoryCV("guid" + i, "Test", "https://www.mozilla.org/", 10L, 10L, 10));
            bundle.putSerializable(BrowserContract.History.VISITS, buildHistoryVisitsCVs(10, "guid" + i, 1L, 3, false));
            recordBundles[i] = bundle;
        }
        data.putSerializable(BrowserContract.METHOD_PARAM_DATA, recordBundles);

        Bundle result = historyClient.call(BrowserContract.METHOD_INSERT_HISTORY_WITH_VISITS_FROM_SYNC, historyTestUri.toString(), data);
        assertNotNull(result);
        assertNull(result.getSerializable(BrowserContract.METHOD_RESULT));
        assertRowCount(historyClient, historyTestUri, insertedRecordCount);
        assertRowCount(visitsClient, visitsTestUri, insertedRecordCount * 10);

        assertHistoryValuesForGuidsFromSync(insertedRecordCount, "Test", "https://www.mozilla.org/", 10L, 10);

        long twoMonthsAgo = System.currentTimeMillis() - 1000L * 60 * 60 * 12 * 60;
        // inserting a new entry sets the date created and modified automatically, so let's reset them
        ContentValues cv = new ContentValues();
        cv.put(BrowserContract.History.DATE_CREATED, twoMonthsAgo);
        cv.put(BrowserContract.History.DATE_MODIFIED, twoMonthsAgo);
        assertEquals(insertedRecordCount, historyClient.update(historyTestUri, cv, null, null));

        historyClient.delete(expireHistoryNormalUri, null, null);

        // Expect that we're left with just 2000 records.
        assertHistoryValuesForGuidsFromSync(2000, "Test", "https://www.mozilla.org/", 10L, 10);
        assertRowCount(visitsClient, visitsTestUri, 2000 * 10);
    }

    /**
     * Tests bulk-inserting history records, and then running a aggressive expiration.
     */
    @Test
    public void testBullkHistoryInsertThenAggressiveExpire() throws Exception {
        final Bundle data = new Bundle();
        // Test insert three history records with 10 visits each.
        final int insertedRecordCount = 1000;

        Bundle[] recordBundles = new Bundle[insertedRecordCount];
        for (int i = 0; i < insertedRecordCount; i++) {
            final Bundle bundle = new Bundle();
            bundle.putParcelable(BrowserContract.METHOD_PARAM_OBJECT, buildHistoryCV("guid" + i, "Test", "https://www.mozilla.org/", 10L, 10L, 10));
            bundle.putSerializable(BrowserContract.History.VISITS, buildHistoryVisitsCVs(10, "guid" + i, 1L, 3, false));
            recordBundles[i] = bundle;
        }
        data.putSerializable(BrowserContract.METHOD_PARAM_DATA, recordBundles);

        Bundle result = historyClient.call(BrowserContract.METHOD_INSERT_HISTORY_WITH_VISITS_FROM_SYNC, historyTestUri.toString(), data);
        assertNotNull(result);
        assertNull(result.getSerializable(BrowserContract.METHOD_RESULT));
        assertRowCount(historyClient, historyTestUri, insertedRecordCount);
        assertRowCount(visitsClient, visitsTestUri, insertedRecordCount * 10);

        assertHistoryValuesForGuidsFromSync(insertedRecordCount, "Test", "https://www.mozilla.org/", 10L, 10);

        // Aggressive expiration doesn't care about record timestamps.
        historyClient.delete(expireHistoryAggressiveUri, null, null);

        // Expect that we're left with just 500 records.
        assertHistoryValuesForGuidsFromSync(500, "Test", "https://www.mozilla.org/", 10L, 10);
        assertRowCount(visitsClient, visitsTestUri, 500 * 10);
    }

    private ContentValues[] buildHistoryVisitsCVs(int numberOfVisits, String guid, long baseDate, int visitType, boolean isLocal) {
        final ContentValues[] visits = new ContentValues[numberOfVisits];
        for (int i = 0; i < numberOfVisits; i++) {
            final ContentValues visit = new ContentValues();
            visit.put(BrowserContract.Visits.HISTORY_GUID, guid);
            visit.put(BrowserContract.Visits.DATE_VISITED, baseDate + i);
            visit.put(BrowserContract.Visits.VISIT_TYPE, visitType);
            visit.put(BrowserContract.Visits.IS_LOCAL, isLocal ? BrowserContract.Visits.VISIT_IS_LOCAL : BrowserContract.Visits.VISIT_IS_REMOTE);
            visits[i] = visit;
        }
        return visits;
    }

    private ContentValues buildHistoryCV(String guid, String title, String url, Long lastVisited, Long remoteLastVisited, Integer visits) {
        ContentValues cv = new ContentValues();
        cv.put(BrowserContract.History.GUID, guid);
        if (title != null) {
            cv.put(BrowserContract.History.TITLE, title);
        }
        cv.put(BrowserContract.History.URL, url);
        if (lastVisited != null) {
            cv.put(BrowserContract.History.DATE_LAST_VISITED, lastVisited);
        }
        if (remoteLastVisited != null) {
            cv.put(BrowserContract.History.REMOTE_DATE_LAST_VISITED, remoteLastVisited);
        }
        if (visits != null) {
            cv.put(BrowserContract.History.VISITS, visits);
            cv.put(BrowserContract.History.REMOTE_VISITS, visits);
        }
        return cv;
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