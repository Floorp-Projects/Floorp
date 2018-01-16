/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.background.db;

import android.content.ContentProvider;
import android.content.ContentProviderClient;
import android.content.ContentValues;
import android.database.Cursor;
import android.net.Uri;
import android.os.RemoteException;

import org.json.simple.JSONArray;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.background.testhelpers.TestRunner;
import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.sync.repositories.android.BrowserContractHelpers;
import org.mozilla.gecko.sync.repositories.android.FennecTabsRepository;
import org.mozilla.gecko.sync.repositories.domain.TabsRecord;
import org.robolectric.shadows.ShadowContentResolver;

@RunWith(TestRunner.class)
public class TestTabsProvider {
    public static final String TEST_CLIENT_GUID = "test guid"; // Real GUIDs never contain spaces.
    public static final String TEST_CLIENT_NAME = "test client name";

    public static final String CLIENTS_GUID_IS = BrowserContract.Clients.GUID + " = ?";
    public static final String TABS_CLIENT_GUID_IS = BrowserContract.Tabs.CLIENT_GUID + " = ?";

    protected Tab testTab1;
    protected Tab testTab2;
    protected Tab testTab3;

    protected ContentProvider provider;

    @Before
    public void setUp() {
        provider = DelegatingTestContentProvider.createDelegatingTabsProvider();
    }

    @After
    public void tearDown() throws Exception {
        provider.shutdown();
        provider = null;
    }

    protected ContentProviderClient getClientsClient() {
        final ShadowContentResolver cr = new ShadowContentResolver();
        return cr.acquireContentProviderClient(BrowserContractHelpers.CLIENTS_CONTENT_URI);
    }

    protected ContentProviderClient getTabsClient() {
        final ShadowContentResolver cr = new ShadowContentResolver();
        return cr.acquireContentProviderClient(BrowserContractHelpers.TABS_CONTENT_URI);
    }

    protected int deleteTestClient(final ContentProviderClient clientsClient) throws RemoteException {
        if (clientsClient == null) {
            throw new IllegalStateException("Provided ContentProviderClient is null");
        }
        return clientsClient.delete(BrowserContractHelpers.CLIENTS_CONTENT_URI, CLIENTS_GUID_IS, new String[] { TEST_CLIENT_GUID });
    }

    protected int deleteAllTestTabs(final ContentProviderClient tabsClient) throws RemoteException {
        if (tabsClient == null) {
            throw new IllegalStateException("Provided ContentProviderClient is null");
        }
        return tabsClient.delete(BrowserContractHelpers.TABS_CONTENT_URI, TABS_CLIENT_GUID_IS, new String[] { TEST_CLIENT_GUID });
    }

    protected void insertTestClient(final ContentProviderClient clientsClient) throws RemoteException {
        ContentValues cv = new ContentValues();
        cv.put(BrowserContract.Clients.GUID, TEST_CLIENT_GUID);
        cv.put(BrowserContract.Clients.NAME, TEST_CLIENT_NAME);
        clientsClient.insert(BrowserContractHelpers.CLIENTS_CONTENT_URI, cv);
    }

    @SuppressWarnings("unchecked")
    protected void insertSomeTestTabs(ContentProviderClient tabsClient) throws RemoteException {
        final JSONArray history1 = new JSONArray();
        history1.add("http://test.com/test1.html");
        testTab1 = new Tab("test title 1", "http://test.com/test1.png", history1, 1000);

        final JSONArray history2 = new JSONArray();
        history2.add("http://test.com/test2.html#1");
        history2.add("http://test.com/test2.html#2");
        history2.add("http://test.com/test2.html#3");
        testTab2 = new Tab("test title 2", "http://test.com/test2.png", history2, 2000);

        final JSONArray history3 = new JSONArray();
        history3.add("http://test.com/test3.html#1");
        history3.add("http://test.com/test3.html#2");
        testTab3 = new Tab("test title 3", "http://test.com/test3.png", history3, 3000);

        tabsClient.insert(BrowserContractHelpers.TABS_CONTENT_URI, testTab1.toContentValues(TEST_CLIENT_GUID, 0));
        tabsClient.insert(BrowserContractHelpers.TABS_CONTENT_URI, testTab2.toContentValues(TEST_CLIENT_GUID, 1));
        tabsClient.insert(BrowserContractHelpers.TABS_CONTENT_URI, testTab3.toContentValues(TEST_CLIENT_GUID, 2));
    }

    // Sanity.
    @Test
    public void testObtainCP() {
        final ContentProviderClient clientsClient = getClientsClient();
        Assert.assertNotNull(clientsClient);
        clientsClient.release();

        final ContentProviderClient tabsClient = getTabsClient();
        Assert.assertNotNull(tabsClient);
        tabsClient.release();
    }

    @Test
    public void testDeleteEmptyClients() throws RemoteException {
        final Uri uri = BrowserContractHelpers.CLIENTS_CONTENT_URI;
        final ContentProviderClient clientsClient = getClientsClient();

        // Have to ensure that it's empty…
        clientsClient.delete(uri, null, null);

        int deleted = clientsClient.delete(uri, null, null);
        Assert.assertEquals(0, deleted);
    }

    @Test
    public void testDeleteEmptyTabs() throws RemoteException {
        final ContentProviderClient tabsClient = getTabsClient();

        // Have to ensure that it's empty…
        deleteAllTestTabs(tabsClient);

        int deleted = deleteAllTestTabs(tabsClient);
        Assert.assertEquals(0, deleted);
    }

    @Test
    public void testStoreAndRetrieveClients() throws RemoteException {
        final Uri uri = BrowserContractHelpers.CLIENTS_CONTENT_URI;
        final ContentProviderClient clientsClient = getClientsClient();

        // Have to ensure that it's empty…
        clientsClient.delete(uri, null, null);

        final long now = System.currentTimeMillis();
        final ContentValues first = new ContentValues();
        final ContentValues second = new ContentValues();
        first.put(BrowserContract.Clients.GUID, "abcdefghijkl");
        first.put(BrowserContract.Clients.NAME, "Frist Psot");
        first.put(BrowserContract.Clients.LAST_MODIFIED, now + 1);
        second.put(BrowserContract.Clients.GUID, "mnopqrstuvwx");
        second.put(BrowserContract.Clients.NAME, "Second!!1!");
        second.put(BrowserContract.Clients.LAST_MODIFIED, now + 2);

        ContentValues[] values = new ContentValues[] { first, second };
        final int inserted = clientsClient.bulkInsert(uri, values);
        Assert.assertEquals(2, inserted);

        final String since = BrowserContract.Clients.LAST_MODIFIED + " >= ?";
        final String[] nowArg = new String[] { String.valueOf(now) };
        final String guidAscending = BrowserContract.Clients.GUID + " ASC";
        Cursor cursor = clientsClient.query(uri, null, since, nowArg, guidAscending);

        Assert.assertNotNull(cursor);
        try {
            Assert.assertTrue(cursor.moveToFirst());
            Assert.assertEquals(2, cursor.getCount());

            final String g1 = cursor.getString(cursor.getColumnIndexOrThrow(BrowserContract.Clients.GUID));
            final String n1 = cursor.getString(cursor.getColumnIndexOrThrow(BrowserContract.Clients.NAME));
            final long m1   = cursor.getLong(cursor.getColumnIndexOrThrow(BrowserContract.Clients.LAST_MODIFIED));
            Assert.assertEquals(first.get(BrowserContract.Clients.GUID), g1);
            Assert.assertEquals(first.get(BrowserContract.Clients.NAME), n1);
            Assert.assertEquals(now + 1, m1);

            Assert.assertTrue(cursor.moveToNext());
            final String g2 = cursor.getString(cursor.getColumnIndexOrThrow(BrowserContract.Clients.GUID));
            final String n2 = cursor.getString(cursor.getColumnIndexOrThrow(BrowserContract.Clients.NAME));
            final long m2   = cursor.getLong(cursor.getColumnIndexOrThrow(BrowserContract.Clients.LAST_MODIFIED));
            Assert.assertEquals(second.get(BrowserContract.Clients.GUID), g2);
            Assert.assertEquals(second.get(BrowserContract.Clients.NAME), n2);
            Assert.assertEquals(now + 2, m2);

            Assert.assertFalse(cursor.moveToNext());
        } finally {
            cursor.close();
        }

        int deleted = clientsClient.delete(uri, null, null);
        Assert.assertEquals(2, deleted);
    }

    @Test
    public void testTabFromCursor() throws Exception {
        final ContentProviderClient tabsClient = getTabsClient();
        final ContentProviderClient clientsClient = getClientsClient();

        deleteAllTestTabs(tabsClient);
        deleteTestClient(clientsClient);
        insertTestClient(clientsClient);
        insertSomeTestTabs(tabsClient);

        final String positionAscending = BrowserContract.Tabs.POSITION + " ASC";
        Cursor cursor = null;
        try {
            cursor = tabsClient.query(BrowserContractHelpers.TABS_CONTENT_URI, null, TABS_CLIENT_GUID_IS, new String[] { TEST_CLIENT_GUID }, positionAscending);
            Assert.assertEquals(3, cursor.getCount());

            cursor.moveToFirst();
            final Tab parsed1 = Tab.fromCursor(cursor);
            Assert.assertEquals(testTab1, parsed1);

            cursor.moveToNext();
            final Tab parsed2 = Tab.fromCursor(cursor);
            Assert.assertEquals(testTab2, parsed2);

            cursor.moveToPosition(2);
            final Tab parsed3 = Tab.fromCursor(cursor);
            Assert.assertEquals(testTab3, parsed3);
        } finally {
            cursor.close();
        }
    }

    @Test
    public void testDeletingClientDeletesTabs() throws Exception {
        final ContentProviderClient tabsClient = getTabsClient();
        final ContentProviderClient clientsClient = getClientsClient();

        deleteAllTestTabs(tabsClient);
        deleteTestClient(clientsClient);
        insertTestClient(clientsClient);
        insertSomeTestTabs(tabsClient);

        // Delete just the client...
        clientsClient.delete(BrowserContractHelpers.CLIENTS_CONTENT_URI, CLIENTS_GUID_IS, new String [] { TEST_CLIENT_GUID });

        Cursor cursor = null;
        try {
            cursor = tabsClient.query(BrowserContractHelpers.TABS_CONTENT_URI, null, TABS_CLIENT_GUID_IS, new String[] { TEST_CLIENT_GUID }, null);
            // ... and all that client's tabs should be removed.
            Assert.assertEquals(0, cursor.getCount());
        } finally {
            cursor.close();
        }
    }

    @Test
    public void testTabsRecordFromCursor() throws Exception {
        final ContentProviderClient tabsClient = getTabsClient();

        deleteAllTestTabs(tabsClient);
        insertTestClient(getClientsClient());
        insertSomeTestTabs(tabsClient);

        final String positionAscending = BrowserContract.Tabs.POSITION + " ASC";
        Cursor cursor = null;
        try {
            cursor = tabsClient.query(BrowserContractHelpers.TABS_CONTENT_URI, null, TABS_CLIENT_GUID_IS, new String[] { TEST_CLIENT_GUID }, positionAscending);
            Assert.assertEquals(3, cursor.getCount());

            cursor.moveToPosition(1);

            final TabsRecord tabsRecord = FennecTabsRepository.tabsRecordFromCursor(cursor, TEST_CLIENT_GUID, TEST_CLIENT_NAME, 1234);

            // Make sure we clean up after ourselves.
            Assert.assertEquals(1, cursor.getPosition());

            Assert.assertEquals(TEST_CLIENT_GUID, tabsRecord.guid);
            Assert.assertEquals(TEST_CLIENT_NAME, tabsRecord.clientName);

            Assert.assertEquals(3, tabsRecord.tabs.size());
            Assert.assertEquals(testTab1, tabsRecord.tabs.get(0));
            Assert.assertEquals(testTab2, tabsRecord.tabs.get(1));
            Assert.assertEquals(testTab3, tabsRecord.tabs.get(2));

            Assert.assertEquals(1234, tabsRecord.lastModified);
        } finally {
            cursor.close();
        }
    }

    // Verify that we can fetch a record when there are no local tabs at all.
    @Test
    public void testEmptyTabsRecordFromCursor() throws Exception {
        final ContentProviderClient tabsClient = getTabsClient();

        deleteAllTestTabs(tabsClient);

        final String positionAscending = BrowserContract.Tabs.POSITION + " ASC";
        Cursor cursor = null;
        try {
            cursor = tabsClient.query(BrowserContractHelpers.TABS_CONTENT_URI, null, TABS_CLIENT_GUID_IS, new String[] { TEST_CLIENT_GUID }, positionAscending);
            Assert.assertEquals(0, cursor.getCount());

            final TabsRecord tabsRecord = FennecTabsRepository.tabsRecordFromCursor(cursor, TEST_CLIENT_GUID, TEST_CLIENT_NAME, 1234);

            Assert.assertEquals(TEST_CLIENT_GUID, tabsRecord.guid);
            Assert.assertEquals(TEST_CLIENT_NAME, tabsRecord.clientName);

            Assert.assertNotNull(tabsRecord.tabs);
            Assert.assertEquals(0, tabsRecord.tabs.size());

            Assert.assertEquals(1234, tabsRecord.lastModified);
        } finally {
            cursor.close();
        }
    }

    // Not much of a test, but verifies the tabs record at least agrees with the
    // disk data and doubles as a database inspector.
    @Test
    public void testLocalTabs() throws Exception {
        final ContentProviderClient tabsClient = getTabsClient();

        final String positionAscending = BrowserContract.Tabs.POSITION + " ASC";
        Cursor cursor = null;
        try {
            // Keep this in sync with the Fennec schema.
            cursor = tabsClient.query(BrowserContractHelpers.TABS_CONTENT_URI, null, BrowserContract.Tabs.CLIENT_GUID + " IS NULL", null, positionAscending);
            CursorDumper.dumpCursor(cursor);

            final TabsRecord tabsRecord = FennecTabsRepository.tabsRecordFromCursor(cursor, TEST_CLIENT_GUID, TEST_CLIENT_NAME, 1234);

            Assert.assertEquals(TEST_CLIENT_GUID, tabsRecord.guid);
            Assert.assertEquals(TEST_CLIENT_NAME, tabsRecord.clientName);

            Assert.assertNotNull(tabsRecord.tabs);
            Assert.assertEquals(cursor.getCount(), tabsRecord.tabs.size());
        } finally {
            cursor.close();
        }
    }
}
