/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.background.db;

import java.util.concurrent.ExecutorService;

import org.mozilla.gecko.background.helpers.AndroidSyncTestCase;
import org.mozilla.gecko.background.sync.helpers.ExpectFetchDelegate;
import org.mozilla.gecko.background.sync.helpers.ExpectFetchSinceDelegate;
import org.mozilla.gecko.background.sync.helpers.ExpectGuidsSinceDelegate;
import org.mozilla.gecko.background.sync.helpers.ExpectNoStoreDelegate;
import org.mozilla.gecko.background.sync.helpers.ExpectStoredDelegate;
import org.mozilla.gecko.background.sync.helpers.SessionTestHelper;
import org.mozilla.gecko.background.testhelpers.WaitHelper;
import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.sync.repositories.InactiveSessionException;
import org.mozilla.gecko.sync.repositories.NoContentProviderException;
import org.mozilla.gecko.sync.repositories.NoStoreDelegateException;
import org.mozilla.gecko.sync.repositories.RepositorySession;
import org.mozilla.gecko.sync.repositories.android.BrowserContractHelpers;
import org.mozilla.gecko.sync.repositories.android.FormHistoryRepositorySession;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionCreationDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionStoreDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionWipeDelegate;
import org.mozilla.gecko.sync.repositories.domain.FormHistoryRecord;
import org.mozilla.gecko.sync.repositories.domain.Record;

import android.content.ContentProviderClient;
import android.content.ContentValues;
import android.content.Context;
import android.database.Cursor;
import android.net.Uri;
import android.os.RemoteException;

public class TestFormHistoryRepositorySession extends AndroidSyncTestCase {
  protected ContentProviderClient formsProvider = null;

  public TestFormHistoryRepositorySession() throws NoContentProviderException {
    super();
  }

  public void setUp() {
    if (formsProvider == null) {
      try {
        formsProvider = FormHistoryRepositorySession.acquireContentProvider(getApplicationContext());
      } catch (NoContentProviderException e) {
        fail("Failed to acquireContentProvider: " + e);
      }
    }

    try {
      FormHistoryRepositorySession.purgeDatabases(formsProvider);
    } catch (RemoteException e) {
      fail("Failed to purgeDatabases: " + e);
    }
  }

  public void tearDown() {
    if (formsProvider != null) {
      formsProvider.release();
      formsProvider = null;
    }
  }

  protected FormHistoryRepositorySession.FormHistoryRepository getRepository() {
    /**
     * Override this chain in order to avoid our test code having to create two
     * sessions all the time.
     */
    return new FormHistoryRepositorySession.FormHistoryRepository() {
      @Override
      public void createSession(RepositorySessionCreationDelegate delegate,
                                Context context) {
        try {
          final FormHistoryRepositorySession session = new FormHistoryRepositorySession(this, context) {
            @Override
            protected synchronized void trackGUID(String guid) {
            }
          };
          delegate.onSessionCreated(session);
        } catch (Exception e) {
          delegate.onSessionCreateFailed(e);
        }
      }
    };
  }


  protected FormHistoryRepositorySession createSession() {
    return (FormHistoryRepositorySession) SessionTestHelper.createSession(
        getApplicationContext(),
        getRepository());
  }

  protected FormHistoryRepositorySession createAndBeginSession() {
    return (FormHistoryRepositorySession) SessionTestHelper.createAndBeginSession(
        getApplicationContext(),
        getRepository());
  }

  public void testAcquire() throws NoContentProviderException {
    final FormHistoryRepositorySession session = createAndBeginSession();
    assertNotNull(session.getFormsProvider());
    session.abort();
  }

  protected int numRecords(FormHistoryRepositorySession session, Uri uri) throws RemoteException {
    Cursor cur = null;
    try {
      cur = session.getFormsProvider().query(uri, null, null, null, null);
      return cur.getCount();
    } finally {
      if (cur != null) {
        cur.close();
      }
    }
  }

  protected long after0;
  protected long after1;
  protected long after2;
  protected long after3;
  protected long after4;
  protected FormHistoryRecord regular1;
  protected FormHistoryRecord regular2;
  protected FormHistoryRecord deleted1;
  protected FormHistoryRecord deleted2;

  public void insertTwoRecords(FormHistoryRepositorySession session) throws RemoteException {
    Uri regularUri = BrowserContractHelpers.FORM_HISTORY_CONTENT_URI;
    Uri deletedUri = BrowserContractHelpers.DELETED_FORM_HISTORY_CONTENT_URI;
    after0 = System.currentTimeMillis();

    regular1 = new FormHistoryRecord("guid1", "forms", System.currentTimeMillis(), false);
    regular1.fieldName = "fieldName1";
    regular1.fieldValue = "value1";
    final ContentValues cv1 = new ContentValues();
    cv1.put(BrowserContract.FormHistory.GUID, regular1.guid);
    cv1.put(BrowserContract.FormHistory.FIELD_NAME, regular1.fieldName);
    cv1.put(BrowserContract.FormHistory.VALUE, regular1.fieldValue);
    cv1.put(BrowserContract.FormHistory.FIRST_USED, 1000 * regular1.lastModified); // Microseconds.

    int regularInserted = session.getFormsProvider().bulkInsert(regularUri, new ContentValues[] { cv1 });
    assertEquals(1, regularInserted);
    after1 = System.currentTimeMillis();

    deleted1 = new FormHistoryRecord("guid3", "forms", -1, true);
    final ContentValues cv3 = new ContentValues();
    cv3.put(BrowserContract.FormHistory.GUID, deleted1.guid);
    // cv3.put(BrowserContract.DeletedFormHistory.TIME_DELETED, record3.lastModified); // Set by CP.

    int deletedInserted = session.getFormsProvider().bulkInsert(deletedUri, new ContentValues[] { cv3 });
    assertEquals(1, deletedInserted);
    after2 = System.currentTimeMillis();

    regular2 = null;
    deleted2 = null;
  }

  public void insertFourRecords(FormHistoryRepositorySession session) throws RemoteException {
    Uri regularUri = BrowserContractHelpers.FORM_HISTORY_CONTENT_URI;
    Uri deletedUri = BrowserContractHelpers.DELETED_FORM_HISTORY_CONTENT_URI;

    insertTwoRecords(session);

    regular2 = new FormHistoryRecord("guid2", "forms", System.currentTimeMillis(), false);
    regular2.fieldName = "fieldName2";
    regular2.fieldValue = "value2";
    final ContentValues cv2 = new ContentValues();
    cv2.put(BrowserContract.FormHistory.GUID, regular2.guid);
    cv2.put(BrowserContract.FormHistory.FIELD_NAME, regular2.fieldName);
    cv2.put(BrowserContract.FormHistory.VALUE, regular2.fieldValue);
    cv2.put(BrowserContract.FormHistory.FIRST_USED, 1000 * regular2.lastModified); // Microseconds.

    int regularInserted = session.getFormsProvider().bulkInsert(regularUri, new ContentValues[] { cv2 });
    assertEquals(1, regularInserted);
    after3 = System.currentTimeMillis();

    deleted2 = new FormHistoryRecord("guid4", "forms", -1, true);
    final ContentValues cv4 = new ContentValues();
    cv4.put(BrowserContract.FormHistory.GUID, deleted2.guid);
    // cv4.put(BrowserContract.DeletedFormHistory.TIME_DELETED, record4.lastModified); // Set by CP.

    int deletedInserted = session.getFormsProvider().bulkInsert(deletedUri, new ContentValues[] { cv4 });
    assertEquals(1, deletedInserted);
    after4 = System.currentTimeMillis();
  }

  public void testWipe() throws NoContentProviderException, RemoteException {
    final FormHistoryRepositorySession session = createAndBeginSession();

    insertTwoRecords(session);
    assertTrue(numRecords(session, BrowserContractHelpers.FORM_HISTORY_CONTENT_URI) > 0);
    assertTrue(numRecords(session, BrowserContractHelpers.DELETED_FORM_HISTORY_CONTENT_URI) > 0);

    performWait(WaitHelper.onThreadRunnable(new Runnable() {
      @Override
      public void run() {
        session.wipe(new RepositorySessionWipeDelegate() {
          public void onWipeSucceeded() {
            performNotify();
          }
          public void onWipeFailed(Exception ex) {
            performNotify("Wipe should have succeeded", ex);
          }
          @Override
          public RepositorySessionWipeDelegate deferredWipeDelegate(final ExecutorService executor) {
            return this;
          }
        });
      }
    }));

    assertEquals(0, numRecords(session, BrowserContractHelpers.FORM_HISTORY_CONTENT_URI));
    assertEquals(0, numRecords(session, BrowserContractHelpers.DELETED_FORM_HISTORY_CONTENT_URI));

    session.abort();
  }

  protected Runnable fetchSinceRunnable(final RepositorySession session, final long timestamp, final String[] expectedGuids) {
    return new Runnable() {
      @Override
      public void run() {
        session.fetchSince(timestamp, new ExpectFetchSinceDelegate(timestamp, expectedGuids));
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

  protected Runnable fetchRunnable(final RepositorySession session, final String[] guids, final Record[] expectedRecords) {
    return new Runnable() {
      @Override
      public void run() {
        try {
          session.fetch(guids, new ExpectFetchDelegate(expectedRecords));
        } catch (InactiveSessionException e) {
          performNotify(e);
        }
      }
    };
  }

  public void testFetchAll() throws NoContentProviderException, RemoteException {
    final FormHistoryRepositorySession session = createAndBeginSession();

    insertTwoRecords(session);

    performWait(fetchAllRunnable(session, new Record[] { regular1, deleted1 }));

    session.abort();
  }

  public void testFetchByGuid() throws NoContentProviderException, RemoteException {
    final FormHistoryRepositorySession session = createAndBeginSession();

    insertTwoRecords(session);

    performWait(fetchRunnable(session,
        new String[] { regular1.guid, deleted1.guid },
        new Record[] { regular1, deleted1 }));
    performWait(fetchRunnable(session,
        new String[] { regular1.guid },
        new Record[] { regular1 }));
    performWait(fetchRunnable(session,
        new String[] { deleted1.guid, "NON_EXISTENT_GUID?" },
        new Record[] { deleted1 }));
    performWait(fetchRunnable(session,
        new String[] { "FIRST_NON_EXISTENT_GUID", "SECOND_NON_EXISTENT_GUID?" },
        new Record[] { }));

    session.abort();
  }

  public void testFetchSince() throws NoContentProviderException, RemoteException {
    final FormHistoryRepositorySession session = createAndBeginSession();

    insertFourRecords(session);

    performWait(fetchSinceRunnable(session,
        after0, new String[] { regular1.guid, deleted1.guid, regular2.guid, deleted2.guid }));
    performWait(fetchSinceRunnable(session,
        after1, new String[] { deleted1.guid, regular2.guid, deleted2.guid }));
    performWait(fetchSinceRunnable(session,
        after2, new String[] { regular2.guid, deleted2.guid }));
    performWait(fetchSinceRunnable(session,
        after3, new String[] { deleted2.guid }));
    performWait(fetchSinceRunnable(session,
        after4, new String[] { }));

    session.abort();
  }

  protected Runnable guidsSinceRunnable(final RepositorySession session, final long timestamp, final String[] expectedGuids) {
    return new Runnable() {
      @Override
      public void run() {
        session.guidsSince(timestamp, new ExpectGuidsSinceDelegate(expectedGuids));
      }
    };
  }

  public void testGuidsSince() throws NoContentProviderException, RemoteException {
    final FormHistoryRepositorySession session = createAndBeginSession();

    insertTwoRecords(session);

    performWait(guidsSinceRunnable(session,
        after0, new String[] { regular1.guid, deleted1.guid }));
    performWait(guidsSinceRunnable(session,
        after1, new String[] { deleted1.guid}));
    performWait(guidsSinceRunnable(session,
        after2, new String[] { }));

    session.abort();
  }

  protected Runnable storeRunnable(final RepositorySession session, final Record record, final RepositorySessionStoreDelegate delegate) {
    return new Runnable() {
      @Override
      public void run() {
        session.setStoreDelegate(delegate);
        try {
          session.store(record);
          session.storeDone();
        } catch (NoStoreDelegateException e) {
          performNotify("NoStoreDelegateException should not occur.", e);
        }
      }
    };
  }

  public void testStoreRemoteNew() throws NoContentProviderException, RemoteException {
    final FormHistoryRepositorySession session = createAndBeginSession();

    insertTwoRecords(session);

    FormHistoryRecord rec;

    // remote regular, local missing => should store.
    rec = new FormHistoryRecord("new1", "forms", System.currentTimeMillis(), false);
    rec.fieldName  = "fieldName1";
    rec.fieldValue = "fieldValue1";
    performWait(storeRunnable(session, rec, new ExpectStoredDelegate(rec.guid)));
    performWait(fetchRunnable(session, new String[] { rec.guid }, new Record[] { rec }));

    // remote deleted, local missing => should delete, but at the moment we ignore.
    rec = new FormHistoryRecord("new2", "forms", System.currentTimeMillis(), true);
    performWait(storeRunnable(session, rec, new ExpectNoStoreDelegate()));
    performWait(fetchRunnable(session, new String[] { rec.guid }, new Record[] { }));

    session.abort();
  }

  public void testStoreRemoteNewer() throws NoContentProviderException, RemoteException {
    final FormHistoryRepositorySession session = createAndBeginSession();

    insertFourRecords(session);
    long newTimestamp = System.currentTimeMillis();

    FormHistoryRecord rec;

    // remote regular, local regular, remote newer => should update.
    rec = new FormHistoryRecord(regular1.guid, regular1.collection, newTimestamp, false);
    rec.fieldName  = regular1.fieldName;
    rec.fieldValue = regular1.fieldValue + "NEW";
    performWait(storeRunnable(session, rec, new ExpectStoredDelegate(rec.guid)));
    performWait(fetchRunnable(session, new String[] { regular1.guid }, new Record[] { rec }));

    // remote deleted, local regular, remote newer => should delete everything.
    rec = new FormHistoryRecord(regular2.guid, regular2.collection, newTimestamp, true);
    performWait(storeRunnable(session, rec, new ExpectStoredDelegate(rec.guid)));
    performWait(fetchRunnable(session, new String[] { regular2.guid }, new Record[] { }));

    // remote regular, local deleted, remote newer => should update.
    rec = new FormHistoryRecord(deleted1.guid, deleted1.collection, newTimestamp, false);
    rec.fieldName  = regular1.fieldName;
    rec.fieldValue = regular1.fieldValue + "NEW";
    performWait(storeRunnable(session, rec, new ExpectStoredDelegate(rec.guid)));
    performWait(fetchRunnable(session, new String[] { deleted1.guid }, new Record[] { rec }));

    // remote deleted, local deleted, remote newer => should delete everything.
    rec = new FormHistoryRecord(deleted2.guid, deleted2.collection, newTimestamp, true);
    performWait(storeRunnable(session, rec, new ExpectNoStoreDelegate()));
    performWait(fetchRunnable(session, new String[] { deleted2.guid }, new Record[] { }));

    session.abort();
  }

  public void testStoreRemoteOlder() throws NoContentProviderException, RemoteException {
    final FormHistoryRepositorySession session = createAndBeginSession();

    long oldTimestamp = System.currentTimeMillis() - 100;
    insertFourRecords(session);

    FormHistoryRecord rec;

    // remote regular, local regular, remote older => should ignore.
    rec = new FormHistoryRecord(regular1.guid, regular1.collection, oldTimestamp, false);
    rec.fieldName  = regular1.fieldName;
    rec.fieldValue = regular1.fieldValue + "NEW";
    performWait(storeRunnable(session, rec, new ExpectNoStoreDelegate()));

    // remote deleted, local regular, remote older => should ignore.
    rec = new FormHistoryRecord(regular2.guid, regular2.collection, oldTimestamp, true);
    performWait(storeRunnable(session, rec, new ExpectNoStoreDelegate()));

    // remote regular, local deleted, remote older => should ignore.
    rec = new FormHistoryRecord(deleted1.guid, deleted1.collection, oldTimestamp, false);
    rec.fieldName  = regular1.fieldName;
    rec.fieldValue = regular1.fieldValue + "NEW";
    performWait(storeRunnable(session, rec, new ExpectNoStoreDelegate()));

    // remote deleted, local deleted, remote older => should ignore.
    rec = new FormHistoryRecord(deleted2.guid, deleted2.collection, oldTimestamp, true);
    performWait(storeRunnable(session, rec, new ExpectNoStoreDelegate()));

    session.abort();
  }

  public void testStoreDifferentGuid() throws NoContentProviderException, RemoteException {
    final FormHistoryRepositorySession session = createAndBeginSession();

    insertTwoRecords(session);

    FormHistoryRecord rec = (FormHistoryRecord) regular1.copyWithIDs("distinct", 999);
    performWait(storeRunnable(session, rec, new ExpectStoredDelegate(rec.guid)));
    // Existing record should take remote record's GUID.
    performWait(fetchAllRunnable(session, new Record[] { rec, deleted1 }));

    session.abort();
  }
}
