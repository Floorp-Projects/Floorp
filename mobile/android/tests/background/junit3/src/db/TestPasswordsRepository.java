/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.background.db;

import java.util.HashSet;
import java.util.Set;

import org.mozilla.gecko.background.helpers.AndroidSyncTestCase;
import org.mozilla.gecko.background.sync.helpers.ExpectFetchDelegate;
import org.mozilla.gecko.background.sync.helpers.ExpectFetchSinceDelegate;
import org.mozilla.gecko.background.sync.helpers.ExpectGuidsSinceDelegate;
import org.mozilla.gecko.background.sync.helpers.ExpectStoredDelegate;
import org.mozilla.gecko.background.sync.helpers.PasswordHelpers;
import org.mozilla.gecko.background.sync.helpers.SessionTestHelper;
import org.mozilla.gecko.background.testhelpers.WaitHelper;
import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.sync.Utils;
import org.mozilla.gecko.sync.repositories.InactiveSessionException;
import org.mozilla.gecko.sync.repositories.NoStoreDelegateException;
import org.mozilla.gecko.sync.repositories.Repository;
import org.mozilla.gecko.sync.repositories.RepositorySession;
import org.mozilla.gecko.sync.repositories.android.BrowserContractHelpers;
import org.mozilla.gecko.sync.repositories.android.PasswordsRepositorySession;
import org.mozilla.gecko.sync.repositories.android.RepoUtils;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionCreationDelegate;
import org.mozilla.gecko.sync.repositories.domain.PasswordRecord;
import org.mozilla.gecko.sync.repositories.domain.Record;

import android.content.ContentProviderClient;
import android.content.Context;
import android.database.Cursor;
import android.os.RemoteException;

public class TestPasswordsRepository extends AndroidSyncTestCase {
  private final String NEW_PASSWORD1 = "password";
  private final String NEW_PASSWORD2 = "drowssap";

  @Override
  public void setUp() {
    wipe();
    assertTrue(WaitHelper.getTestWaiter().isIdle());
  }

  public void testFetchAll() {
    RepositorySession session = createAndBeginSession();
    Record[] expected = new Record[] { PasswordHelpers.createPassword1(),
                                       PasswordHelpers.createPassword2() };

    performWait(storeRunnable(session, expected[0]));
    performWait(storeRunnable(session, expected[1]));

    performWait(fetchAllRunnable(session, expected));
    dispose(session);
  }

  public void testGuidsSinceReturnMultipleRecords() {
    RepositorySession session = createAndBeginSession();

    PasswordRecord record1 = PasswordHelpers.createPassword1();
    PasswordRecord record2 = PasswordHelpers.createPassword2();

    updatePassword(NEW_PASSWORD1, record1);
    long timestamp = updatePassword(NEW_PASSWORD2, record2);

    String[] expected = new String[] { record1.guid, record2.guid };

    performWait(storeRunnable(session, record1));
    performWait(storeRunnable(session, record2));

    performWait(guidsSinceRunnable(session, timestamp, expected));
    dispose(session);
  }

  public void testGuidsSinceReturnNoRecords() {
    RepositorySession session = createAndBeginSession();

    //  Store 1 record in the past.
    performWait(storeRunnable(session, PasswordHelpers.createPassword1()));

    String[] expected = {};
    performWait(guidsSinceRunnable(session, System.currentTimeMillis() + 1000, expected));
    dispose(session);
  }

  public void testFetchSinceOneRecord() {
    RepositorySession session = createAndBeginSession();

    // Passwords fetchSince checks timePasswordChanged, not insertion time.
    PasswordRecord record1 = PasswordHelpers.createPassword1();
    long timeModified1 = updatePassword(NEW_PASSWORD1, record1);
    performWait(storeRunnable(session, record1));

    PasswordRecord record2 = PasswordHelpers.createPassword2();
    long timeModified2 = updatePassword(NEW_PASSWORD2, record2);
    performWait(storeRunnable(session, record2));

    String[] expectedOne = new String[] { record2.guid };
    performWait(fetchSinceRunnable(session, timeModified2 - 10, expectedOne));

    String[] expectedBoth = new String[] { record1.guid, record2.guid };
    performWait(fetchSinceRunnable(session, timeModified1 - 10, expectedBoth));

    dispose(session);
  }

  public void testFetchSinceReturnNoRecords() {
   RepositorySession session = createAndBeginSession();

    performWait(storeRunnable(session, PasswordHelpers.createPassword2()));

    long timestamp = System.currentTimeMillis();

    performWait(fetchSinceRunnable(session, timestamp + 2000, new String[] {}));
    dispose(session);
  }

  public void testFetchOneRecordByGuid() {
    RepositorySession session = createAndBeginSession();
    Record record = PasswordHelpers.createPassword1();
    performWait(storeRunnable(session, record));
    performWait(storeRunnable(session, PasswordHelpers.createPassword2()));

    String[] guids = new String[] { record.guid };
    Record[] expected = new Record[] { record };
    performWait(fetchRunnable(session, guids, expected));
    dispose(session);
  }

  public void testFetchMultipleRecordsByGuids() {
    RepositorySession session = createAndBeginSession();
    PasswordRecord record1 = PasswordHelpers.createPassword1();
    PasswordRecord record2 = PasswordHelpers.createPassword2();
    PasswordRecord record3 = PasswordHelpers.createPassword3();

    performWait(storeRunnable(session, record1));
    performWait(storeRunnable(session, record2));
    performWait(storeRunnable(session, record3));

    String[] guids = new String[] { record1.guid, record2.guid };
    Record[] expected = new Record[] { record1, record2 };
    performWait(fetchRunnable(session, guids, expected));
    dispose(session);
  }

  public void testFetchNoRecordByGuid() {
    RepositorySession session = createAndBeginSession();
    Record record = PasswordHelpers.createPassword1();

    performWait(storeRunnable(session, record));
    performWait(fetchRunnable(session,
                              new String[] { Utils.generateGuid() },
                              new Record[] {}));
    dispose(session);
  }

  public void testStore() {
    final RepositorySession session = createAndBeginSession();
    performWait(storeRunnable(session, PasswordHelpers.createPassword1()));
    dispose(session);
  }

  public void testRemoteNewerTimeStamp() {
    final RepositorySession session = createAndBeginSession();

    // Store updated local record.
    PasswordRecord local = PasswordHelpers.createPassword1();
    updatePassword(NEW_PASSWORD1, local, System.currentTimeMillis() - 1000);
    performWait(storeRunnable(session, local));

    // Sync a remote record version that is newer.
    PasswordRecord remote = PasswordHelpers.createPassword2();
    remote.guid = local.guid;
    updatePassword(NEW_PASSWORD2, remote);
    performWait(storeRunnable(session, remote));

    // Make a fetch, expecting only the newer (remote) record.
    performWait(fetchAllRunnable(session, new Record[] { remote }));
    dispose(session);
  }

  public void testLocalNewerTimeStamp() {
    final RepositorySession session = createAndBeginSession();
    // Remote record updated before local record.
    PasswordRecord remote = PasswordHelpers.createPassword1();
    updatePassword(NEW_PASSWORD1, remote, System.currentTimeMillis() - 1000);

    // Store updated local record.
    PasswordRecord local = PasswordHelpers.createPassword2();
    updatePassword(NEW_PASSWORD2, local);
    performWait(storeRunnable(session, local));

    // Sync a remote record version that is older.
    remote.guid = local.guid;
    performWait(storeRunnable(session, remote));

    // Make a fetch, expecting only the newer (local) record.
    performWait(fetchAllRunnable(session, new Record[] { local }));
    dispose(session);
  }

  /*
   * Store two records that are identical except for guid. Expect to find the
   * remote one after reconciling.
   */
  public void testStoreIdenticalExceptGuid() {
    RepositorySession session = createAndBeginSession();
    PasswordRecord record = PasswordHelpers.createPassword1();
    record.guid = "before1";
    // Store record.
    performWait(storeRunnable(session, record));

    // Store same record, but with different guid.
    record.guid = Utils.generateGuid();
    performWait(storeRunnable(session, record));

    performWait(fetchAllRunnable(session, new Record[] { record }));
    dispose(session);

    session = createAndBeginSession();

    PasswordRecord record2 = PasswordHelpers.createPassword2();
    record2.guid = "before2";
    // Store record.
    performWait(storeRunnable(session, record2));

    // Store same record, but with different guid.
    record2.guid = Utils.generateGuid();
    performWait(storeRunnable(session, record2));

    performWait(fetchAllRunnable(session, new Record[] { record, record2 }));
    dispose(session);
  }

  /*
   * Store two records that are identical except for guid when they both point
   * to the same site and there are multiple records for that site. Expect to
   * find the remote one after reconciling.
   */
  public void testStoreIdenticalExceptGuidOnSameSite() {
    RepositorySession session = createAndBeginSession();
    PasswordRecord record1 = PasswordHelpers.createPassword1();
    record1.encryptedUsername = "original";
    record1.guid = "before1";
    PasswordRecord record2 = PasswordHelpers.createPassword1();
    record2.encryptedUsername = "different";
    record1.guid = "before2";
    // Store records.
    performWait(storeRunnable(session, record1));
    performWait(storeRunnable(session, record2));
    performWait(fetchAllRunnable(session, new Record[] { record1, record2 }));

    dispose(session);
    session = createAndBeginSession();

    // Store same records, but with different guids.
    record1.guid = Utils.generateGuid();
    performWait(storeRunnable(session, record1));
    performWait(fetchAllRunnable(session, new Record[] { record1, record2 }));

    record2.guid = Utils.generateGuid();
    performWait(storeRunnable(session, record2));
    performWait(fetchAllRunnable(session, new Record[] { record1, record2 }));

    dispose(session);
  }

  public void testRawFetch() throws RemoteException {
    RepositorySession session = createAndBeginSession();
    Record[] expected = new Record[] { PasswordHelpers.createPassword1(),
                                       PasswordHelpers.createPassword2() };

    performWait(storeRunnable(session, expected[0]));
    performWait(storeRunnable(session, expected[1]));

    ContentProviderClient client = getApplicationContext().getContentResolver().acquireContentProviderClient(BrowserContract.PASSWORDS_AUTHORITY_URI);
    Cursor cursor = client.query(BrowserContractHelpers.PASSWORDS_CONTENT_URI, null, null, null, null);
    assertEquals(2, cursor.getCount());
    cursor.moveToFirst();
    Set<String> guids = new HashSet<String>();
    while (!cursor.isAfterLast()) {
      String guid = RepoUtils.getStringFromCursor(cursor, BrowserContract.Passwords.GUID);
      guids.add(guid);
      cursor.moveToNext();
    }
    cursor.close();
    assertEquals(2, guids.size());
    assertTrue(guids.contains(expected[0].guid));
    assertTrue(guids.contains(expected[1].guid));
    dispose(session);
  }

  // Helper methods.
  private RepositorySession createAndBeginSession() {
    return SessionTestHelper.createAndBeginSession(
        getApplicationContext(),
        getRepository());
  }

  private Repository getRepository() {
    /**
     * Override this chain in order to avoid our test code having to create two
     * sessions all the time. Don't track records, so they filtering doesn't happen.
     */
    return new PasswordsRepositorySession.PasswordsRepository() {
      @Override
      public void createSession(RepositorySessionCreationDelegate delegate,
          Context context) {
        PasswordsRepositorySession session;
        session = new PasswordsRepositorySession(this, context) {
          @Override
          protected synchronized void trackGUID(String guid) {
          }
        };
        delegate.onSessionCreated(session);
      }
    };
  }

  private void wipe() {
    Context context = getApplicationContext();
    context.getContentResolver().delete(BrowserContractHelpers.PASSWORDS_CONTENT_URI, null, null);
    context.getContentResolver().delete(BrowserContractHelpers.DELETED_PASSWORDS_CONTENT_URI, null, null);
  }

  private static void dispose(RepositorySession session) {
    if (session != null) {
      session.abort();
    }
  }

  private static long updatePassword(String password, PasswordRecord record, long timestamp) {
    record.encryptedPassword = password;
    long modifiedTime = System.currentTimeMillis();
    record.timePasswordChanged = record.lastModified = modifiedTime;
    return modifiedTime;
  }

  private static long updatePassword(String password, PasswordRecord record) {
    return updatePassword(password, record, System.currentTimeMillis());
  }

  // Runnable Helpers.
  private static Runnable storeRunnable(final RepositorySession session, final Record record) {
    return new Runnable() {
      @Override
      public void run() {
        session.setStoreDelegate(new ExpectStoredDelegate(record.guid));
        try {
          session.store(record);
          session.storeDone();
        } catch (NoStoreDelegateException e) {
          fail("NoStoreDelegateException should not occur.");
        }
      }
    };
  }

  private static Runnable fetchAllRunnable(final RepositorySession session, final Record[] records) {
    return new Runnable() {
      @Override
      public void run() {
        session.fetchAll(new ExpectFetchDelegate(records));
      }
    };
  }

  private static Runnable guidsSinceRunnable(final RepositorySession session, final long timestamp, final String[] expected) {
    return new Runnable() {
      @Override
      public void run() {
        session.guidsSince(timestamp, new ExpectGuidsSinceDelegate(expected));
      }
    };
  }

  private static Runnable fetchSinceRunnable(final RepositorySession session, final long timestamp, final String[] expected) {
    return new Runnable() {
      @Override
      public void run() {
        session.fetchSince(timestamp, new ExpectFetchSinceDelegate(timestamp, expected));
      }
    };
  }

  private static Runnable fetchRunnable(final RepositorySession session, final String[] guids, final Record[] expected) {
    return new Runnable() {
      @Override
      public void run() {
        try {
          session.fetch(guids, new ExpectFetchDelegate(expected));
        } catch (InactiveSessionException e) {
          performNotify(e);
        }
      }
    };
  }
}
