/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.background.db;

import org.json.simple.JSONArray;
import org.mozilla.gecko.background.helpers.AndroidSyncTestCase;
import org.mozilla.gecko.background.sync.helpers.ExpectFetchDelegate;
import org.mozilla.gecko.background.sync.helpers.SessionTestHelper;
import org.mozilla.gecko.background.testhelpers.MockClientsDataDelegate;
import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.db.BrowserContract.Clients;
import org.mozilla.gecko.sync.repositories.NoContentProviderException;
import org.mozilla.gecko.sync.repositories.RepositorySession;
import org.mozilla.gecko.sync.repositories.android.BrowserContractHelpers;
import org.mozilla.gecko.sync.repositories.android.ClientsDatabaseAccessor;
import org.mozilla.gecko.sync.repositories.android.FennecTabsRepository;
import org.mozilla.gecko.sync.repositories.android.FennecTabsRepository.FennecTabsRepositorySession;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionCreationDelegate;
import org.mozilla.gecko.sync.repositories.domain.ClientRecord;
import org.mozilla.gecko.sync.repositories.domain.Record;
import org.mozilla.gecko.sync.repositories.domain.TabsRecord;

import android.content.ContentProviderClient;
import android.content.ContentResolver;
import android.content.Context;
import android.database.Cursor;
import android.os.RemoteException;

public class TestFennecTabsRepositorySession extends AndroidSyncTestCase {
  public static final MockClientsDataDelegate clientsDataDelegate = new MockClientsDataDelegate();
  public static final String TEST_CLIENT_GUID = clientsDataDelegate.getAccountGUID();
  public static final String TEST_CLIENT_NAME = clientsDataDelegate.getClientName();
  public static final String TEST_CLIENT_DEVICE_TYPE = "phablet";

  // Override these to test against data that is not live.
  public static final String TEST_TABS_CLIENT_GUID_IS_LOCAL_SELECTION = BrowserContract.Tabs.CLIENT_GUID + " IS ?";
  public static final String[] TEST_TABS_CLIENT_GUID_IS_LOCAL_SELECTION_ARGS = new String[] { TEST_CLIENT_GUID };

  public static final String TEST_CLIENTS_GUID_IS_LOCAL_SELECTION = BrowserContract.Clients.GUID + " IS ?";
  public static final String[] TEST_CLIENTS_GUID_IS_LOCAL_SELECTION_ARGS = new String[] { TEST_CLIENT_GUID };

  protected ContentProviderClient tabsClient = null;
  protected ContentProviderClient clientsClient = null;

  protected ContentProviderClient getTabsClient() {
    final ContentResolver cr = getApplicationContext().getContentResolver();
    return cr.acquireContentProviderClient(BrowserContractHelpers.TABS_CONTENT_URI);
  }

  protected ContentProviderClient getClientsClient() {
    final ContentResolver cr = getApplicationContext().getContentResolver();
    return cr.acquireContentProviderClient(BrowserContractHelpers.CLIENTS_CONTENT_URI);
  }

  public TestFennecTabsRepositorySession() throws NoContentProviderException {
    super();
  }

  @Override
  public void setUp() {
    if (tabsClient == null) {
      tabsClient = getTabsClient();
    }
    if (clientsClient == null) {
      clientsClient = getClientsClient();
    }
  }

  protected int deleteTestClient(final ContentProviderClient clientsClient) throws RemoteException {
    if (clientsClient == null) {
      return -1;
    }
    return clientsClient.delete(BrowserContractHelpers.CLIENTS_CONTENT_URI, TEST_CLIENTS_GUID_IS_LOCAL_SELECTION, TEST_CLIENTS_GUID_IS_LOCAL_SELECTION_ARGS);
  }

  protected int deleteAllTestTabs(final ContentProviderClient tabsClient) throws RemoteException {
    if (tabsClient == null) {
      return -1;
    }
    return tabsClient.delete(BrowserContractHelpers.TABS_CONTENT_URI,
        TEST_TABS_CLIENT_GUID_IS_LOCAL_SELECTION, TEST_TABS_CLIENT_GUID_IS_LOCAL_SELECTION_ARGS);
  }

  @Override
  protected void tearDown() throws Exception {
    if (tabsClient != null) {
      deleteAllTestTabs(tabsClient);

      tabsClient.release();
      tabsClient = null;
    }

    if (clientsClient != null) {
      deleteTestClient(clientsClient);

      clientsClient.release();
      clientsClient = null;
    }
  }

  protected FennecTabsRepository getRepository() {
    /**
     * Override this chain in order to avoid our test code having to create two
     * sessions all the time.
     */
    return new FennecTabsRepository(clientsDataDelegate) {
      @Override
      public void createSession(RepositorySessionCreationDelegate delegate,
                                Context context) {
        try {
          final FennecTabsRepositorySession session = new FennecTabsRepositorySession(this, context) {
            @Override
            protected synchronized void trackGUID(String guid) {
            }

            @Override
            protected String localClientSelection() {
              return TEST_TABS_CLIENT_GUID_IS_LOCAL_SELECTION;
            }

            @Override
            protected String[] localClientSelectionArgs() {
              return TEST_TABS_CLIENT_GUID_IS_LOCAL_SELECTION_ARGS;
            }
          };
          delegate.onSessionCreated(session);
        } catch (Exception e) {
          delegate.onSessionCreateFailed(e);
        }
      }
    };
  }

  protected FennecTabsRepositorySession createSession() {
    return (FennecTabsRepositorySession) SessionTestHelper.createSession(
        getApplicationContext(),
        getRepository());
  }

  protected FennecTabsRepositorySession createAndBeginSession() {
    return (FennecTabsRepositorySession) SessionTestHelper.createAndBeginSession(
        getApplicationContext(),
        getRepository());
  }

  protected Runnable fetchSinceRunnable(final RepositorySession session, final long timestamp, final Record[] expectedRecords) {
    return new Runnable() {
      @Override
      public void run() {
        session.fetchSince(timestamp, new ExpectFetchDelegate(expectedRecords));
      }
    };
  }

  protected Runnable fetchAllRunnable(final RepositorySession session, final Record[] expectedRecords) {
    return new Runnable() {
      @Override
      public void run() {
        session.fetchAll(new ExpectFetchDelegate(expectedRecords));
      }
    };
  }

  protected Tab testTab1;
  protected Tab testTab2;
  protected Tab testTab3;

  @SuppressWarnings("unchecked")
  private void insertSomeTestTabs(ContentProviderClient tabsClient) throws RemoteException {
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

  protected TabsRecord insertTestTabsAndExtractTabsRecord() throws RemoteException {
    insertSomeTestTabs(tabsClient);

    final String positionAscending = BrowserContract.Tabs.POSITION + " ASC";
    Cursor cursor = null;
    try {
      cursor = tabsClient.query(BrowserContractHelpers.TABS_CONTENT_URI, null,
          TEST_TABS_CLIENT_GUID_IS_LOCAL_SELECTION, TEST_TABS_CLIENT_GUID_IS_LOCAL_SELECTION_ARGS, positionAscending);
      CursorDumper.dumpCursor(cursor);

      final TabsRecord tabsRecord = FennecTabsRepository.tabsRecordFromCursor(cursor, TEST_CLIENT_GUID, TEST_CLIENT_NAME);

      assertEquals(TEST_CLIENT_GUID, tabsRecord.guid);
      assertEquals(TEST_CLIENT_NAME, tabsRecord.clientName);

      assertNotNull(tabsRecord.tabs);
      assertEquals(cursor.getCount(), tabsRecord.tabs.size());

      return tabsRecord;
    } finally {
      cursor.close();
    }
  }

  public void testFetchAll() throws NoContentProviderException, RemoteException {
    final TabsRecord tabsRecord = insertTestTabsAndExtractTabsRecord();

    final FennecTabsRepositorySession session = createAndBeginSession();
    performWait(fetchAllRunnable(session, new Record[] { tabsRecord }));

    session.abort();
  }

  public void testFetchSince() throws NoContentProviderException, RemoteException {
    final TabsRecord tabsRecord = insertTestTabsAndExtractTabsRecord();

    final FennecTabsRepositorySession session = createAndBeginSession();

    // Not all tabs are modified after this, but the record should contain them all.
    performWait(fetchSinceRunnable(session, 1000, new Record[] { tabsRecord }));

    // No tabs are modified after this, but our client name has changed in the interim.
    performWait(fetchSinceRunnable(session, 4000, new Record[] { tabsRecord }));

    // No tabs are modified after this, and our client name hasn't changed, so
    // we shouldn't get a record at all. Note: this runs after our static
    // initializer that sets the client data timestamp.
    final long now = System.currentTimeMillis();
    performWait(fetchSinceRunnable(session, now, new Record[] { }));

    // No tabs are modified after this, but our client name has changed, so
    // again we get a record.
    clientsDataDelegate.setClientName("new client name", System.currentTimeMillis());
    performWait(fetchSinceRunnable(session, now, new Record[] { tabsRecord }));

    session.abort();
  }

  // Verify that storing a tabs record writes a clients record with the correct
  // device type to the Fennec clients provider.
  public void testStore() throws NoContentProviderException, RemoteException {
    // Get a valid tabsRecord to write.
    final TabsRecord tabsRecord = insertTestTabsAndExtractTabsRecord();
    deleteAllTestTabs(tabsClient);
    deleteTestClient(clientsClient);

    final ContentResolver cr = getApplicationContext().getContentResolver();
    final ContentProviderClient clientsClient = cr.acquireContentProviderClient(BrowserContractHelpers.CLIENTS_CONTENT_URI);

    try {
      // This clients DB is not the Fennec DB; it's Sync's own clients DB.
      final ClientsDatabaseAccessor db = new ClientsDatabaseAccessor(getApplicationContext());
      try {
        ClientRecord clientRecord = new ClientRecord(TEST_CLIENT_GUID);
        clientRecord.name = TEST_CLIENT_NAME;
        clientRecord.type = TEST_CLIENT_DEVICE_TYPE;
        db.store(clientRecord);
      } finally {
        db.close();
      }

      final FennecTabsRepositorySession session = createAndBeginSession();
      performWait(AndroidBrowserRepositoryTestCase.storeRunnable(session, tabsRecord));

      session.abort();

      // This store should write Sync's idea of the client's device_type to Fennec's clients CP.
      final Cursor cursor = clientsClient.query(BrowserContractHelpers.CLIENTS_CONTENT_URI, null,
          TEST_CLIENTS_GUID_IS_LOCAL_SELECTION, TEST_CLIENTS_GUID_IS_LOCAL_SELECTION_ARGS, null);
      assertNotNull(cursor);

      try {
        assertTrue(cursor.moveToFirst());
        assertEquals(TEST_CLIENT_GUID, cursor.getString(cursor.getColumnIndex(Clients.GUID)));
        assertEquals(TEST_CLIENT_NAME, cursor.getString(cursor.getColumnIndex(Clients.NAME)));
        assertEquals(TEST_CLIENT_DEVICE_TYPE, cursor.getString(cursor.getColumnIndex(Clients.DEVICE_TYPE)));
        assertTrue(cursor.isLast());
      } finally {
        cursor.close();
      }
    } finally {
      // We can't delete only our test client due to a Fennec CP issue with guid vs. client_guid.
      clientsClient.delete(BrowserContractHelpers.CLIENTS_CONTENT_URI, null, null);
      clientsClient.release();
    }
  }
}
