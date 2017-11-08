/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.db;

import android.content.ContentValues;
import android.database.Cursor;
import android.net.Uri;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.background.testhelpers.TestRunner;

import org.mozilla.gecko.db.BrowserContract.History;

import static org.junit.Assert.*;

@RunWith(TestRunner.class)
/**
 * Testing insertion/deletion of visits as by-product of updating history records through BrowserProvider
 */
public class BrowserProviderHistoryVisitsTest extends BrowserProviderHistoryVisitsTestBase {
    @Test
    /**
     * Testing updating history records without affecting visits
     */
    public void testUpdateNoVisit() throws Exception {
        insertHistoryItem("https://www.mozilla.org", "testGUID");

        Cursor cursor = visitsClient.query(visitsTestUri, null, null, null, null);
        assertNotNull(cursor);
        assertEquals(0, cursor.getCount());
        cursor.close();

        ContentValues historyUpdate = new ContentValues();
        historyUpdate.put(History.TITLE, "Mozilla!");
        assertEquals(1,
                historyClient.update(
                        historyTestUri, historyUpdate, History.URL + " = ?", new String[] {"https://www.mozilla.org"}
                )
        );

        cursor = visitsClient.query(visitsTestUri, null, null, null, null);
        assertNotNull(cursor);
        assertEquals(0, cursor.getCount());
        cursor.close();

        ContentValues historyToInsert = new ContentValues();
        historyToInsert.put(History.URL, "https://www.eff.org");
        assertEquals(1,
                historyClient.update(
                        historyTestUri.buildUpon()
                                .appendQueryParameter(BrowserContract.PARAM_INSERT_IF_NEEDED, "true").build(),
                        historyToInsert, null, null
                )
        );

        cursor = visitsClient.query(visitsTestUri, null, null, null, null);
        assertNotNull(cursor);
        assertEquals(0, cursor.getCount());
        cursor.close();
    }

    @Test
    /**
     * Testing INCREMENT_VISITS flag for multiple history records at once
     */
    public void testUpdateMultipleHistoryIncrementVisit() throws Exception {
        insertHistoryItem("https://www.mozilla.org", "testGUID");
        insertHistoryItem("https://www.mozilla.org", "testGUID2");

        // test that visits get inserted when updating existing history records
        assertEquals(2, historyClient.update(
                historyTestUri.buildUpon()
                        .appendQueryParameter(BrowserContract.PARAM_INCREMENT_VISITS, "true").build(),
                new ContentValues(), History.URL + " = ?", new String[] {"https://www.mozilla.org"}
        ));

        Cursor cursor = visitsClient.query(
                visitsTestUri, new String[] {BrowserContract.Visits.HISTORY_GUID}, null, null, null);
        assertNotNull(cursor);
        assertEquals(2, cursor.getCount());
        assertTrue(cursor.moveToFirst());

        String guid1 = cursor.getString(cursor.getColumnIndex(BrowserContract.Visits.HISTORY_GUID));
        cursor.moveToNext();
        String guid2 = cursor.getString(cursor.getColumnIndex(BrowserContract.Visits.HISTORY_GUID));
        cursor.close();

        assertNotEquals(guid1, guid2);

        assertTrue(guid1.equals("testGUID") || guid1.equals("testGUID2"));
    }

    @Test
    /**
     * Testing INCREMENT_VISITS flag and its interplay with INSERT_IF_NEEDED
     */
    public void testUpdateHistoryIncrementVisit() throws Exception {
        insertHistoryItem("https://www.mozilla.org", "testGUID");

        // test that visit gets inserted when updating an existing histor record
        assertEquals(1, historyClient.update(
                historyTestUri.buildUpon()
                        .appendQueryParameter(BrowserContract.PARAM_INCREMENT_VISITS, "true").build(),
            new ContentValues(), History.URL + " = ?", new String[] {"https://www.mozilla.org"}
        ));

        Cursor cursor = visitsClient.query(
                visitsTestUri, new String[] {BrowserContract.Visits.HISTORY_GUID}, null, null, null);
        assertNotNull(cursor);
        assertEquals(1, cursor.getCount());
        assertTrue(cursor.moveToFirst());
        assertEquals(
                "testGUID",
                cursor.getString(cursor.getColumnIndex(BrowserContract.Visits.HISTORY_GUID))
        );
        cursor.close();

        // test that visit gets inserted when updatingOrInserting a new history record
        ContentValues historyItem = new ContentValues();
        historyItem.put(History.URL, "https://www.eff.org");

        assertEquals(1, historyClient.update(
                historyTestUri.buildUpon()
                        .appendQueryParameter(BrowserContract.PARAM_INCREMENT_VISITS, "true")
                        .appendQueryParameter(BrowserContract.PARAM_INSERT_IF_NEEDED, "true").build(),
                historyItem, null, null
        ));

        cursor = historyClient.query(
                historyTestUri,
                new String[] {History.GUID}, History.URL + " = ?", new String[] {"https://www.eff.org"}, null
        );
        assertNotNull(cursor);
        assertEquals(1, cursor.getCount());
        assertTrue(cursor.moveToFirst());
        String insertedGUID = cursor.getString(cursor.getColumnIndex(History.GUID));
        cursor.close();

        cursor = visitsClient.query(
                visitsTestUri, new String[] {BrowserContract.Visits.HISTORY_GUID}, null, null, null);
        assertNotNull(cursor);
        assertEquals(2, cursor.getCount());
        assertTrue(cursor.moveToFirst());
        assertEquals(insertedGUID,
                cursor.getString(cursor.getColumnIndex(BrowserContract.Visits.HISTORY_GUID))
        );
        cursor.close();
    }

    @Test
    /**
     * Test that for locally generated visits, we store their timestamps in microseconds, and not in
     * milliseconds like history does.
     */
    public void testTimestampConversionOnInsertion() throws Exception {
        insertHistoryItem("https://www.mozilla.org", "testGUID");

        Long lastVisited = System.currentTimeMillis();
        ContentValues updatedVisitedTime = new ContentValues();
        updatedVisitedTime.put(History.DATE_LAST_VISITED, lastVisited);

        // test with last visited date passed in
        assertEquals(1, historyClient.update(
                historyTestUri.buildUpon()
                        .appendQueryParameter(BrowserContract.PARAM_INCREMENT_VISITS, "true").build(),
                updatedVisitedTime, History.URL + " = ?", new String[] {"https://www.mozilla.org"}
        ));

        Cursor cursor = visitsClient.query(visitsTestUri, new String[] {BrowserContract.Visits.DATE_VISITED}, null, null, null);
        assertNotNull(cursor);
        assertEquals(1, cursor.getCount());
        assertTrue(cursor.moveToFirst());

        assertEquals(lastVisited * 1000, cursor.getLong(cursor.getColumnIndex(BrowserContract.Visits.DATE_VISITED)));
        cursor.close();

        // test without last visited date
        assertEquals(1, historyClient.update(
                historyTestUri.buildUpon()
                        .appendQueryParameter(BrowserContract.PARAM_INCREMENT_VISITS, "true").build(),
                new ContentValues(), History.URL + " = ?", new String[] {"https://www.mozilla.org"}
        ));

        cursor = visitsClient.query(visitsTestUri, new String[] {BrowserContract.Visits.DATE_VISITED}, null, null, null);
        assertNotNull(cursor);
        assertEquals(2, cursor.getCount());
        assertTrue(cursor.moveToFirst());

        // CP should generate time off of current time upon insertion and convert to microseconds.
        // This also tests correct ordering (DESC on date).
        assertTrue(lastVisited * 1000 < cursor.getLong(cursor.getColumnIndex(BrowserContract.Visits.DATE_VISITED)));
        cursor.close();
    }

    @Test
    /**
     * This should perform `DELETE FROM visits WHERE history_guid in IN (?, ?, ?, ..., ?)` sort of statement
     * SQLite has a variable count limit (999 by default), so we're testing here that our deletion
     * code does the right thing and chunks deletes to account for this limitation.
     */
    public void testDeletingLotsOfHistory() throws Exception {
        Uri incrementUri = historyTestUri.buildUpon()
                .appendQueryParameter(BrowserContract.PARAM_INCREMENT_VISITS, "true").build();

        // insert bunch of history records, and for each insert a visit
        for (int i = 0; i < 2100; i++) {
            final String url = "https://www.mozilla" + i + ".org";
            insertHistoryItem(url, "testGUID" + i);
            assertEquals(1, historyClient.update(incrementUri, new ContentValues(), History.URL + " = ?", new String[] {url}));
        }

        // sanity check
        Cursor cursor = visitsClient.query(visitsTestUri, null, null, null, null);
        assertNotNull(cursor);
        assertEquals(2100, cursor.getCount());
        cursor.close();

        // delete all of the history items - this will trigger chunked deletion of visits as well
        assertEquals(2100,
                historyClient.delete(historyTestUri, null, null)
        );

        // check that all visits where deleted
        cursor = visitsClient.query(visitsTestUri, null, null, null, null);
        assertNotNull(cursor);
        assertEquals(0, cursor.getCount());
        cursor.close();
    }

    @Test
    /**
     * Test visit deletion as by-product of history deletion - both explicit (from outside of Sync),
     * and implicit (cascaded, from Sync).
     */
    public void testDeletingHistory() throws Exception {
        insertHistoryItem("https://www.mozilla.org", "testGUID");
        insertHistoryItem("https://www.eff.org", "testGUID2");

        // insert some visits
        assertEquals(1, historyClient.update(
                historyTestUri.buildUpon()
                        .appendQueryParameter(BrowserContract.PARAM_INCREMENT_VISITS, "true").build(),
                new ContentValues(), History.URL + " = ?", new String[] {"https://www.mozilla.org"}
        ));
        assertEquals(1, historyClient.update(
                historyTestUri.buildUpon()
                        .appendQueryParameter(BrowserContract.PARAM_INCREMENT_VISITS, "true").build(),
                new ContentValues(), History.URL + " = ?", new String[] {"https://www.mozilla.org"}
        ));
        assertEquals(1, historyClient.update(
                historyTestUri.buildUpon()
                        .appendQueryParameter(BrowserContract.PARAM_INCREMENT_VISITS, "true").build(),
                new ContentValues(), History.URL + " = ?", new String[] {"https://www.eff.org"}
        ));

        Cursor cursor = visitsClient.query(visitsTestUri, null, null, null, null);
        assertNotNull(cursor);
        assertEquals(3, cursor.getCount());
        cursor.close();

        // test that corresponding visit records are deleted if Sync isn't involved
        assertEquals(1,
                historyClient.delete(historyTestUri, History.URL + " = ?", new String[] {"https://www.mozilla.org"})
        );

        cursor = visitsClient.query(visitsTestUri, null, null, null, null);
        assertNotNull(cursor);
        assertEquals(1, cursor.getCount());
        cursor.close();

        // test that corresponding visit records are deleted if Sync is involved
        // insert some more visits
        ContentValues moz = new ContentValues();
        moz.put(History.URL, "https://www.mozilla.org");
        moz.put(History.GUID, "testGUID3");
        assertEquals(1, historyClient.update(
                historyTestUri.buildUpon()
                        .appendQueryParameter(BrowserContract.PARAM_INCREMENT_VISITS, "true")
                        .appendQueryParameter(BrowserContract.PARAM_INSERT_IF_NEEDED, "true").build(),
                moz, History.URL + " = ?", new String[] {"https://www.mozilla.org"}
        ));
        assertEquals(1, historyClient.update(
                historyTestUri.buildUpon()
                        .appendQueryParameter(BrowserContract.PARAM_INCREMENT_VISITS, "true")
                        .appendQueryParameter(BrowserContract.PARAM_INSERT_IF_NEEDED, "true").build(),
                new ContentValues(), History.URL + " = ?", new String[] {"https://www.eff.org"}
        ));

        assertEquals(1,
                historyClient.delete(
                        historyTestUri.buildUpon().appendQueryParameter(BrowserContract.PARAM_IS_SYNC, "true").build(),
                        History.URL + " = ?", new String[] {"https://www.eff.org"})
        );

        cursor = visitsClient.query(visitsTestUri, new String[] {BrowserContract.Visits.HISTORY_GUID}, null, null, null);
        assertNotNull(cursor);
        assertEquals(1, cursor.getCount());
        assertTrue(cursor.moveToFirst());
        assertEquals("testGUID3", cursor.getString(cursor.getColumnIndex(BrowserContract.Visits.HISTORY_GUID)));
        cursor.close();
    }

    @Test
    /**
     * Test that changes to History GUID are cascaded to individual visits.
     * See UPDATE CASCADED on Visit's HISTORY_GUID foreign key.
     */
    public void testHistoryGUIDUpdate() throws Exception {
        insertHistoryItem("https://www.mozilla.org", "testGUID");
        insertHistoryItem("https://www.eff.org", "testGUID2");

        // insert some visits
        assertEquals(1, historyClient.update(
                historyTestUri.buildUpon()
                        .appendQueryParameter(BrowserContract.PARAM_INCREMENT_VISITS, "true").build(),
                new ContentValues(), History.URL + " = ?", new String[] {"https://www.mozilla.org"}
        ));
        assertEquals(1, historyClient.update(
                historyTestUri.buildUpon()
                        .appendQueryParameter(BrowserContract.PARAM_INCREMENT_VISITS, "true").build(),
                new ContentValues(), History.URL + " = ?", new String[] {"https://www.mozilla.org"}
        ));

        // change testGUID -> testGUIDNew
        ContentValues newGuid = new ContentValues();
        newGuid.put(History.GUID, "testGUIDNew");
        assertEquals(1, historyClient.update(
                historyTestUri, newGuid, History.URL + " = ?", new String[] {"https://www.mozilla.org"}
        ));

        Cursor cursor = visitsClient.query(visitsTestUri, null, BrowserContract.Visits.HISTORY_GUID + " = ?", new String[] {"testGUIDNew"}, null);
        assertNotNull(cursor);
        assertEquals(2, cursor.getCount());
        cursor.close();
    }
}