/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.sync.middleware.test;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;

import java.io.IOException;
import java.io.UnsupportedEncodingException;

import junit.framework.AssertionFailedError;

import org.json.simple.parser.ParseException;
import org.junit.Before;
import org.junit.Test;
import org.mozilla.android.sync.test.helpers.ExpectSuccessRepositorySessionBeginDelegate;
import org.mozilla.android.sync.test.helpers.ExpectSuccessRepositorySessionCreationDelegate;
import org.mozilla.android.sync.test.helpers.ExpectSuccessRepositorySessionFetchRecordsDelegate;
import org.mozilla.android.sync.test.helpers.ExpectSuccessRepositorySessionFinishDelegate;
import org.mozilla.android.sync.test.helpers.ExpectSuccessRepositorySessionStoreDelegate;
import org.mozilla.android.sync.test.helpers.ExpectSuccessRepositoryWipeDelegate;
import org.mozilla.gecko.background.testhelpers.MockRecord;
import org.mozilla.gecko.background.testhelpers.WBORepository;
import org.mozilla.gecko.background.testhelpers.WaitHelper;
import org.mozilla.gecko.sync.CryptoRecord;
import org.mozilla.gecko.sync.NonObjectJSONException;
import org.mozilla.gecko.sync.crypto.CryptoException;
import org.mozilla.gecko.sync.crypto.KeyBundle;
import org.mozilla.gecko.sync.middleware.Crypto5MiddlewareRepository;
import org.mozilla.gecko.sync.middleware.Crypto5MiddlewareRepositorySession;
import org.mozilla.gecko.sync.repositories.InactiveSessionException;
import org.mozilla.gecko.sync.repositories.InvalidSessionTransitionException;
import org.mozilla.gecko.sync.repositories.NoStoreDelegateException;
import org.mozilla.gecko.sync.repositories.RepositorySession;
import org.mozilla.gecko.sync.repositories.domain.BookmarkRecord;
import org.mozilla.gecko.sync.repositories.domain.Record;

public class TestCrypto5MiddlewareRepositorySession {
  public static WaitHelper getTestWaiter() {
    return WaitHelper.getTestWaiter();
  }

  public static void performWait(Runnable runnable) {
    getTestWaiter().performWait(runnable);
  }

  protected static void performNotify(InactiveSessionException e) {
    final AssertionFailedError failed = new AssertionFailedError("Inactive session.");
    failed.initCause(e);
    getTestWaiter().performNotify(failed);
  }

  protected static void performNotify(InvalidSessionTransitionException e) {
    final AssertionFailedError failed = new AssertionFailedError("Invalid session transition.");
    failed.initCause(e);
    getTestWaiter().performNotify(failed);
  }

  public Runnable onThreadRunnable(Runnable runnable) {
    return WaitHelper.onThreadRunnable(runnable);
  }

  public WBORepository wboRepo;
  public KeyBundle keyBundle;
  public Crypto5MiddlewareRepository cmwRepo;
  public Crypto5MiddlewareRepositorySession cmwSession;

  @Before
  public void setUp() throws CryptoException {
    wboRepo = new WBORepository();
    keyBundle = KeyBundle.withRandomKeys();
    cmwRepo = new Crypto5MiddlewareRepository(wboRepo, keyBundle);
    cmwSession = null;
  }

  /**
   * Run `runnable` in performWait(... onBeginSucceeded { } ).
   *
   * The Crypto5MiddlewareRepositorySession is available in self.cmwSession.
   *
   * @param runnable
   */
  public void runInOnBeginSucceeded(final Runnable runnable) {
    final TestCrypto5MiddlewareRepositorySession self = this;
    performWait(onThreadRunnable(new Runnable() {
      @Override
      public void run() {
        cmwRepo.createSession(new ExpectSuccessRepositorySessionCreationDelegate(getTestWaiter()) {
          @Override
          public void onSessionCreated(RepositorySession session) {
            self.cmwSession = (Crypto5MiddlewareRepositorySession)session;
            assertSame(RepositorySession.SessionStatus.UNSTARTED, cmwSession.getStatus());

            try {
              session.begin(new ExpectSuccessRepositorySessionBeginDelegate(getTestWaiter()) {
                @Override
                public void onBeginSucceeded(RepositorySession _session) {
                  assertSame(self.cmwSession, _session);
                  runnable.run();
                }
              });
            } catch (InvalidSessionTransitionException e) {
              TestCrypto5MiddlewareRepositorySession.performNotify(e);
            }
          }
        }, null);
      }
    }));
  }

  @Test
  /**
   * Verify that the status is actually being advanced.
   */
  public void testStatus() {
    runInOnBeginSucceeded(new Runnable() {
      @Override public void run() {
        assertSame(RepositorySession.SessionStatus.ACTIVE, cmwSession.getStatus());
        try {
          cmwSession.finish(new ExpectSuccessRepositorySessionFinishDelegate(getTestWaiter()));
        } catch (InactiveSessionException e) {
          performNotify(e);
        }
      }
    });
    assertSame(RepositorySession.SessionStatus.DONE, cmwSession.getStatus());
  }

  @Test
  /**
   * Verify that wipe is actually wiping the underlying repository.
   */
  public void testWipe() {
    Record record = new MockRecord("nncdefghiaaa", "coll", System.currentTimeMillis(), false);
    wboRepo.wbos.put(record.guid, record);
    assertEquals(1, wboRepo.wbos.size());

    runInOnBeginSucceeded(new Runnable() {
      @Override public void run() {
        cmwSession.wipe(new ExpectSuccessRepositoryWipeDelegate(getTestWaiter()));
      }
    });
    performWait(onThreadRunnable(new Runnable() {
      @Override public void run() {
        try {
          cmwSession.finish(new ExpectSuccessRepositorySessionFinishDelegate(getTestWaiter()));
        } catch (InactiveSessionException e) {
          performNotify(e);
        }
      }
    }));
    assertEquals(0, wboRepo.wbos.size());
  }

  @Test
  /**
   * Verify that store is actually writing encrypted data to the underlying repository.
   */
  public void testStoreEncrypts() throws NonObjectJSONException, CryptoException, IOException, ParseException {
    final BookmarkRecord record = new BookmarkRecord("nncdefghiaaa", "coll", System.currentTimeMillis(), false);
    record.title = "unencrypted title";

    runInOnBeginSucceeded(new Runnable() {
      @Override public void run() {
        try {
          try {
            cmwSession.setStoreDelegate(new ExpectSuccessRepositorySessionStoreDelegate(getTestWaiter()));
            cmwSession.store(record);
          } catch (NoStoreDelegateException e) {
            getTestWaiter().performNotify(new AssertionFailedError("Should not happen."));
          }
          cmwSession.storeDone();
          cmwSession.finish(new ExpectSuccessRepositorySessionFinishDelegate(getTestWaiter()));
        } catch (InactiveSessionException e) {
          performNotify(e);
        }
      }
    });
    assertEquals(1, wboRepo.wbos.size());
    assertTrue(wboRepo.wbos.containsKey(record.guid));

    Record storedRecord = wboRepo.wbos.get(record.guid);
    CryptoRecord cryptoRecord = (CryptoRecord)storedRecord;
    assertSame(cryptoRecord.keyBundle, keyBundle);

    cryptoRecord = cryptoRecord.decrypt();
    BookmarkRecord decryptedRecord = new BookmarkRecord();
    decryptedRecord.initFromEnvelope(cryptoRecord);
    assertEquals(record.title, decryptedRecord.title);
  }

  @Test
  /**
   * Verify that fetch is actually retrieving encrypted data from the underlying repository and is correctly decrypting it.
   */
  public void testFetchDecrypts() throws UnsupportedEncodingException, CryptoException {
    final BookmarkRecord record1 = new BookmarkRecord("nncdefghiaaa", "coll", System.currentTimeMillis(), false);
    record1.title = "unencrypted title";
    final BookmarkRecord record2 = new BookmarkRecord("XXXXXXXXXXXX", "coll", System.currentTimeMillis(), false);
    record2.title = "unencrypted second title";

    CryptoRecord encryptedRecord1 = record1.getEnvelope();
    encryptedRecord1.keyBundle = keyBundle;
    encryptedRecord1 = encryptedRecord1.encrypt();
    wboRepo.wbos.put(record1.guid, encryptedRecord1);

    CryptoRecord encryptedRecord2 = record2.getEnvelope();
    encryptedRecord2.keyBundle = keyBundle;
    encryptedRecord2 = encryptedRecord2.encrypt();
    wboRepo.wbos.put(record2.guid, encryptedRecord2);

    final ExpectSuccessRepositorySessionFetchRecordsDelegate fetchRecordsDelegate = new ExpectSuccessRepositorySessionFetchRecordsDelegate(getTestWaiter());
    runInOnBeginSucceeded(new Runnable() {
      @Override public void run() {
        try {
          cmwSession.fetch(new String[] { record1.guid }, fetchRecordsDelegate);
        } catch (InactiveSessionException e) {
          performNotify(e);
        }
      }
    });
    performWait(onThreadRunnable(new Runnable() {
      @Override public void run() {
        try {
          cmwSession.finish(new ExpectSuccessRepositorySessionFinishDelegate(getTestWaiter()));
        } catch (InactiveSessionException e) {
          performNotify(e);
        }
      }
    }));

    assertEquals(1, fetchRecordsDelegate.fetchedRecords.size());
    BookmarkRecord decryptedRecord = new BookmarkRecord();
    decryptedRecord.initFromEnvelope((CryptoRecord)fetchRecordsDelegate.fetchedRecords.get(0));
    assertEquals(record1.title, decryptedRecord.title);
  }

  @Test
  /**
   * Verify that fetchAll is actually retrieving encrypted data from the underlying repository and is correctly decrypting it.
   */
  public void testFetchAllDecrypts() throws UnsupportedEncodingException, CryptoException {
    final BookmarkRecord record1 = new BookmarkRecord("nncdefghiaaa", "coll", System.currentTimeMillis(), false);
    record1.title = "unencrypted title";
    final BookmarkRecord record2 = new BookmarkRecord("XXXXXXXXXXXX", "coll", System.currentTimeMillis(), false);
    record2.title = "unencrypted second title";

    CryptoRecord encryptedRecord1 = record1.getEnvelope();
    encryptedRecord1.keyBundle = keyBundle;
    encryptedRecord1 = encryptedRecord1.encrypt();
    wboRepo.wbos.put(record1.guid, encryptedRecord1);

    CryptoRecord encryptedRecord2 = record2.getEnvelope();
    encryptedRecord2.keyBundle = keyBundle;
    encryptedRecord2 = encryptedRecord2.encrypt();
    wboRepo.wbos.put(record2.guid, encryptedRecord2);

    final ExpectSuccessRepositorySessionFetchRecordsDelegate fetchAllRecordsDelegate = new ExpectSuccessRepositorySessionFetchRecordsDelegate(getTestWaiter());
    runInOnBeginSucceeded(new Runnable() {
      @Override public void run() {
        cmwSession.fetchAll(fetchAllRecordsDelegate);
      }
    });
    performWait(onThreadRunnable(new Runnable() {
      @Override public void run() {
        try {
          cmwSession.finish(new ExpectSuccessRepositorySessionFinishDelegate(getTestWaiter()));
        } catch (InactiveSessionException e) {
          performNotify(e);
        }
      }
    }));

    assertEquals(2, fetchAllRecordsDelegate.fetchedRecords.size());
    BookmarkRecord decryptedRecord1 = new BookmarkRecord();
    decryptedRecord1.initFromEnvelope((CryptoRecord)fetchAllRecordsDelegate.fetchedRecords.get(0));
    BookmarkRecord decryptedRecord2 = new BookmarkRecord();
    decryptedRecord2.initFromEnvelope((CryptoRecord)fetchAllRecordsDelegate.fetchedRecords.get(1));

    // We should get two different decrypted records
    assertFalse(decryptedRecord1.guid.equals(decryptedRecord2.guid));
    assertFalse(decryptedRecord1.title.equals(decryptedRecord2.title));
    // And we should know about both.
    assertTrue(record1.title.equals(decryptedRecord1.title) || record1.title.equals(decryptedRecord2.title));
    assertTrue(record2.title.equals(decryptedRecord1.title) || record2.title.equals(decryptedRecord2.title));
  }
}
