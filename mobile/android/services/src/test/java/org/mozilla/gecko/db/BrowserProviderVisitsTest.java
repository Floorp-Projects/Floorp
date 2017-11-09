/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.db;

import android.content.ContentValues;
import android.database.Cursor;
import android.net.Uri;
import android.os.RemoteException;

import static org.junit.Assert.*;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.background.testhelpers.TestRunner;
import org.mozilla.gecko.db.BrowserContract.Visits;

@RunWith(TestRunner.class)
/**
 * Testing direct interactions with visits through BrowserProvider
 */
public class BrowserProviderVisitsTest extends BrowserProviderHistoryVisitsTestBase {
    @Test
    /**
     * Test that default visit parameters are set on insert.
     */
    public void testDefaultVisit() throws RemoteException {
        String url = "https://www.mozilla.org";
        String guid = "testGuid";

        assertNotNull(insertHistoryItem(url, guid));

        ContentValues visitItem = new ContentValues();
        Long visitedDate = System.currentTimeMillis();
        visitItem.put(Visits.HISTORY_GUID, guid);
        visitItem.put(Visits.DATE_VISITED, visitedDate);
        Uri insertedVisitUri = visitsClient.insert(visitsTestUri, visitItem);
        assertNotNull(insertedVisitUri);

        Cursor cursor = visitsClient.query(visitsTestUri, null, null, null, null);
        assertNotNull(cursor);
        try {
            assertTrue(cursor.moveToFirst());
            String insertedGuid = cursor.getString(cursor.getColumnIndex(Visits.HISTORY_GUID));
            assertEquals(guid, insertedGuid);

            Long insertedDate = cursor.getLong(cursor.getColumnIndex(Visits.DATE_VISITED));
            assertEquals(visitedDate, insertedDate);

            Integer insertedType = cursor.getInt(cursor.getColumnIndex(Visits.VISIT_TYPE));
            assertEquals(insertedType, Integer.valueOf(1));

            Integer insertedIsLocal = cursor.getInt(cursor.getColumnIndex(Visits.IS_LOCAL));
            assertEquals(insertedIsLocal, Integer.valueOf(1));
        } finally {
            cursor.close();
        }
    }

    @Test
    /**
     * Test that we can't insert visit for non-existing GUID.
     */
    public void testMissingHistoryGuid() throws RemoteException {
        ContentValues visitItem = new ContentValues();
        visitItem.put(Visits.HISTORY_GUID, "blah");
        visitItem.put(Visits.DATE_VISITED, System.currentTimeMillis());
        assertNull(visitsClient.insert(visitsTestUri, visitItem));
    }

    @Test
    /**
     * Test that visit insert uses non-conflict insert.
     */
    public void testNonConflictInsert() throws RemoteException {
        String url = "https://www.mozilla.org";
        String guid = "testGuid";

        assertNotNull(insertHistoryItem(url, guid));

        ContentValues visitItem = new ContentValues();
        Long visitedDate = System.currentTimeMillis();
        visitItem.put(Visits.HISTORY_GUID, guid);
        visitItem.put(Visits.DATE_VISITED, visitedDate);
        Uri insertedVisitUri = visitsClient.insert(visitsTestUri, visitItem);
        assertNotNull(insertedVisitUri);

        Uri insertedVisitUri2 = visitsClient.insert(visitsTestUri, visitItem);
        assertEquals(insertedVisitUri, insertedVisitUri2);
    }

    @Test
    /**
     * Test that non-default visit parameters won't get overridden.
     */
    public void testNonDefaultInsert() throws RemoteException {
        assertNotNull(insertHistoryItem("https://www.mozilla.org", "testGuid"));

        Integer typeToInsert = 5;
        Integer isLocalToInsert = 0;

        ContentValues visitItem = new ContentValues();
        visitItem.put(Visits.HISTORY_GUID, "testGuid");
        visitItem.put(Visits.DATE_VISITED, System.currentTimeMillis());
        visitItem.put(Visits.VISIT_TYPE, typeToInsert);
        visitItem.put(Visits.IS_LOCAL, isLocalToInsert);

        assertNotNull(visitsClient.insert(visitsTestUri, visitItem));

        Cursor cursor = visitsClient.query(visitsTestUri, null, null, null, null);
        assertNotNull(cursor);
        try {
            assertTrue(cursor.moveToFirst());

            Integer insertedVisitType = cursor.getInt(cursor.getColumnIndex(Visits.VISIT_TYPE));
            assertEquals(typeToInsert, insertedVisitType);

            Integer insertedIsLocal = cursor.getInt(cursor.getColumnIndex(Visits.IS_LOCAL));
            assertEquals(isLocalToInsert, insertedIsLocal);
        } finally {
            cursor.close();
        }
    }

    @Test
    /**
     * Test that default sorting order (DATE_VISITED DESC) is set if we don't specify any sorting params
     */
    public void testDefaultSortingOrder() throws RemoteException {
        assertNotNull(insertHistoryItem("https://www.mozilla.org", "testGuid"));

        Long time1 = System.currentTimeMillis();
        Long time2 = time1 + 100;
        Long time3 = time1 + 200;

        ContentValues visitItem = new ContentValues();
        visitItem.put(Visits.DATE_VISITED, time1);
        visitItem.put(Visits.HISTORY_GUID, "testGuid");
        assertNotNull(visitsClient.insert(visitsTestUri, visitItem));

        visitItem.put(Visits.DATE_VISITED, time3);
        assertNotNull(visitsClient.insert(visitsTestUri, visitItem));

        visitItem.put(Visits.DATE_VISITED, time2);
        assertNotNull(visitsClient.insert(visitsTestUri, visitItem));

        Cursor cursor = visitsClient.query(visitsTestUri, null, null, null, null);
        assertNotNull(cursor);
        try {
            assertEquals(3, cursor.getCount());
            assertTrue(cursor.moveToFirst());

            Long timeInserted = cursor.getLong(cursor.getColumnIndex(Visits.DATE_VISITED));
            assertEquals(time3, timeInserted);

            cursor.moveToNext();

            timeInserted = cursor.getLong(cursor.getColumnIndex(Visits.DATE_VISITED));
            assertEquals(time2, timeInserted);

            cursor.moveToNext();

            timeInserted = cursor.getLong(cursor.getColumnIndex(Visits.DATE_VISITED));
            assertEquals(time1, timeInserted);
        } finally {
            cursor.close();
        }
    }

    @Test
    /**
     * Test that if we pass sorting params, they're not overridden
     */
    public void testNonDefaultSortingOrder() throws RemoteException {
        assertNotNull(insertHistoryItem("https://www.mozilla.org", "testGuid"));

        Long time1 = System.currentTimeMillis();
        Long time2 = time1 + 100;
        Long time3 = time1 + 200;

        ContentValues visitItem = new ContentValues();
        visitItem.put(Visits.DATE_VISITED, time1);
        visitItem.put(Visits.HISTORY_GUID, "testGuid");
        assertNotNull(visitsClient.insert(visitsTestUri, visitItem));

        visitItem.put(Visits.DATE_VISITED, time3);
        assertNotNull(visitsClient.insert(visitsTestUri, visitItem));

        visitItem.put(Visits.DATE_VISITED, time2);
        assertNotNull(visitsClient.insert(visitsTestUri, visitItem));

        Cursor cursor = visitsClient.query(visitsTestUri, null, null, null, Visits.DATE_VISITED + " ASC");
        assertNotNull(cursor);
        assertEquals(3, cursor.getCount());
        assertTrue(cursor.moveToFirst());

        Long timeInserted = cursor.getLong(cursor.getColumnIndex(Visits.DATE_VISITED));
        assertEquals(time1, timeInserted);

        cursor.moveToNext();

        timeInserted = cursor.getLong(cursor.getColumnIndex(Visits.DATE_VISITED));
        assertEquals(time2, timeInserted);

        cursor.moveToNext();

        timeInserted = cursor.getLong(cursor.getColumnIndex(Visits.DATE_VISITED));
        assertEquals(time3, timeInserted);

        cursor.close();
    }

    @Test
    /**
     * Tests deletion of all visits, and by some selection (GUID, IS_LOCAL)
     */
    public void testVisitDeletion() throws RemoteException {
        assertNotNull(insertHistoryItem("https://www.mozilla.org", "testGuid"));
        assertNotNull(insertHistoryItem("https://www.eff.org", "testGuid2"));

        Long time1 = System.currentTimeMillis();

        ContentValues visitItem = new ContentValues();
        visitItem.put(Visits.DATE_VISITED, time1);
        visitItem.put(Visits.HISTORY_GUID, "testGuid");
        assertNotNull(visitsClient.insert(visitsTestUri, visitItem));

        visitItem = new ContentValues();
        visitItem.put(Visits.DATE_VISITED, time1 + 100);
        visitItem.put(Visits.HISTORY_GUID, "testGuid");
        assertNotNull(visitsClient.insert(visitsTestUri, visitItem));

        ContentValues visitItem2 = new ContentValues();
        visitItem2.put(Visits.DATE_VISITED, time1);
        visitItem2.put(Visits.HISTORY_GUID, "testGuid2");
        assertNotNull(visitsClient.insert(visitsTestUri, visitItem2));

        Cursor cursor = visitsClient.query(visitsTestUri, null, null, null, null);
        assertNotNull(cursor);
        assertEquals(3, cursor.getCount());
        cursor.close();

        assertEquals(3, visitsClient.delete(visitsTestUri, null, null));

        cursor = visitsClient.query(visitsTestUri, null, null, null, null);
        assertNotNull(cursor);
        assertEquals(0, cursor.getCount());
        cursor.close();

        // test selective deletion - by IS_LOCAL
        visitItem = new ContentValues();
        visitItem.put(Visits.DATE_VISITED, time1);
        visitItem.put(Visits.HISTORY_GUID, "testGuid");
        visitItem.put(Visits.IS_LOCAL, 0);
        assertNotNull(visitsClient.insert(visitsTestUri, visitItem));

        visitItem = new ContentValues();
        visitItem.put(Visits.DATE_VISITED, time1 + 100);
        visitItem.put(Visits.HISTORY_GUID, "testGuid");
        visitItem.put(Visits.IS_LOCAL, 1);
        assertNotNull(visitsClient.insert(visitsTestUri, visitItem));

        visitItem2 = new ContentValues();
        visitItem2.put(Visits.DATE_VISITED, time1);
        visitItem2.put(Visits.HISTORY_GUID, "testGuid2");
        visitItem2.put(Visits.IS_LOCAL, 0);
        assertNotNull(visitsClient.insert(visitsTestUri, visitItem2));

        cursor = visitsClient.query(visitsTestUri, null, null, null, null);
        assertNotNull(cursor);
        assertEquals(3, cursor.getCount());
        cursor.close();

        assertEquals(2,
                visitsClient.delete(visitsTestUri, Visits.IS_LOCAL + " = ?", new String[]{"0"}));
        cursor = visitsClient.query(visitsTestUri, null, null, null, null);
        assertNotNull(cursor);
        assertEquals(1, cursor.getCount());
        assertTrue(cursor.moveToFirst());
        assertEquals(time1 + 100, cursor.getLong(cursor.getColumnIndex(Visits.DATE_VISITED)));
        assertEquals("testGuid", cursor.getString(cursor.getColumnIndex(Visits.HISTORY_GUID)));
        assertEquals(1, cursor.getInt(cursor.getColumnIndex(Visits.IS_LOCAL)));
        cursor.close();

        // test selective deletion - by HISTORY_GUID
        assertNotNull(visitsClient.insert(visitsTestUri, visitItem2));
        cursor = visitsClient.query(visitsTestUri, null, null, null, null);
        assertNotNull(cursor);
        assertEquals(2, cursor.getCount());
        cursor.close();

        assertEquals(1,
                visitsClient.delete(visitsTestUri, Visits.HISTORY_GUID + " = ?", new String[]{"testGuid"}));
        cursor = visitsClient.query(visitsTestUri, null, null, null, null);
        assertNotNull(cursor);
        assertEquals(1, cursor.getCount());
        assertTrue(cursor.moveToFirst());
        assertEquals("testGuid2", cursor.getString(cursor.getColumnIndex(Visits.HISTORY_GUID)));
        cursor.close();
    }
}