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
            insertHistoryItem(url, "testGUID" + i, baseTime - i);
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