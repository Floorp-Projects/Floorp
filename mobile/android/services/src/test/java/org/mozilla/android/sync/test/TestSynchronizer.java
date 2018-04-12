/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.android.sync.test;

import android.content.Context;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.android.sync.test.SynchronizerHelpers.TrackingWBORepository;
import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.background.testhelpers.WBORepository;
import org.mozilla.gecko.background.testhelpers.WaitHelper;
import org.mozilla.gecko.sync.repositories.FetchFailedException;
import org.mozilla.gecko.sync.repositories.RepositorySessionBundle;
import org.mozilla.gecko.sync.repositories.StoreFailedException;
import org.mozilla.gecko.sync.repositories.domain.BookmarkRecord;
import org.mozilla.gecko.sync.synchronizer.Synchronizer;
import org.mozilla.gecko.sync.synchronizer.SynchronizerDelegate;
import org.mozilla.gecko.sync.synchronizer.SynchronizerSession;
import org.mozilla.gecko.sync.synchronizer.SynchronizerSessionDelegate;
import org.robolectric.RobolectricTestRunner;

import java.util.ArrayList;
import java.util.Date;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

@RunWith(RobolectricTestRunner.class)
public class TestSynchronizer {
  public static final String LOG_TAG = "TestSynchronizer";

  public static void assertInRangeInclusive(long earliest, long value, long latest) {
    assertTrue(earliest <= value);
    assertTrue(latest   >= value);
  }

  public static void recordEquals(BookmarkRecord r, String guid, long lastModified, boolean deleted, String collection) {
    assertEquals(r.guid,         guid);
    assertEquals(r.lastModified, lastModified);
    assertEquals(r.deleted,      deleted);
    assertEquals(r.collection,   collection);
  }

  public static void recordEquals(BookmarkRecord a, BookmarkRecord b) {
    assertEquals(a.guid,         b.guid);
    assertEquals(a.lastModified, b.lastModified);
    assertEquals(a.deleted,      b.deleted);
    assertEquals(a.collection,   b.collection);
  }

  @Before
  public void setUp() {
    WaitHelper.resetTestWaiter();
  }

  @After
  public void tearDown() {
    WaitHelper.resetTestWaiter();
  }

  @Test
  public void testSynchronizerSession() {
    final Context context = null;
    final WBORepository repoA = new TrackingWBORepository();
    final WBORepository repoB = new TrackingWBORepository();

    final String collection  = "bookmarks";
    final boolean deleted    = false;
    final String guidA       = "abcdabcdabcd";
    final String guidB       = "ffffffffffff";
    final String guidC       = "xxxxxxxxxxxx";
    final long lastModifiedA = 312345;
    final long lastModifiedB = 412340;
    final long lastModifiedC = 412345;
    BookmarkRecord bookmarkRecordA = new BookmarkRecord(guidA, collection, lastModifiedA, deleted);
    BookmarkRecord bookmarkRecordB = new BookmarkRecord(guidB, collection, lastModifiedB, deleted);
    BookmarkRecord bookmarkRecordC = new BookmarkRecord(guidC, collection, lastModifiedC, deleted);

    repoA.wbos.put(guidA, bookmarkRecordA);
    repoB.wbos.put(guidB, bookmarkRecordB);
    repoB.wbos.put(guidC, bookmarkRecordC);
    Synchronizer synchronizer = new Synchronizer();
    synchronizer.repositoryA = repoA;
    synchronizer.repositoryB = repoB;
    final SynchronizerSession syncSession = new SynchronizerSession(synchronizer, new SynchronizerSessionDelegate() {
      @Override
      public void onSynchronized(SynchronizerSession session) {
        try {
          assertEquals(1, session.getInboundCount());
          assertEquals(2, session.getOutboundCount());
          WaitHelper.getTestWaiter().performNotify();
        } catch (Throwable e) {
          WaitHelper.getTestWaiter().performNotify(e);
        }
      }

      @Override
      public void onSynchronizeFailed(SynchronizerSession session,
                                      Exception lastException, String reason) {
        WaitHelper.getTestWaiter().performNotify(lastException);
      }

      @Override
      public void onSynchronizeSkipped(SynchronizerSession synchronizerSession) {
        WaitHelper.getTestWaiter().performNotify(new RuntimeException());
      }
    });

    WaitHelper.getTestWaiter().performWait(new Runnable() {
      @Override
      public void run() {
        assertFalse(repoA.wbos.containsKey(guidB));
        assertFalse(repoA.wbos.containsKey(guidC));
        assertFalse(repoB.wbos.containsKey(guidA));
        assertTrue(repoA.wbos.containsKey(guidA));
        assertTrue(repoB.wbos.containsKey(guidB));
        assertTrue(repoB.wbos.containsKey(guidC));
        syncSession.initAndSynchronize(context, new RepositorySessionBundle(0), new RepositorySessionBundle(0));
      }
    });

    // Verify contents.
    assertTrue(repoA.wbos.containsKey(guidA));
    assertTrue(repoA.wbos.containsKey(guidB));
    assertTrue(repoA.wbos.containsKey(guidC));
    assertTrue(repoB.wbos.containsKey(guidA));
    assertTrue(repoB.wbos.containsKey(guidB));
    assertTrue(repoB.wbos.containsKey(guidC));
    BookmarkRecord aa = (BookmarkRecord) repoA.wbos.get(guidA);
    BookmarkRecord ab = (BookmarkRecord) repoA.wbos.get(guidB);
    BookmarkRecord ac = (BookmarkRecord) repoA.wbos.get(guidC);
    BookmarkRecord ba = (BookmarkRecord) repoB.wbos.get(guidA);
    BookmarkRecord bb = (BookmarkRecord) repoB.wbos.get(guidB);
    BookmarkRecord bc = (BookmarkRecord) repoB.wbos.get(guidC);
    recordEquals(aa, guidA, lastModifiedA, deleted, collection);
    recordEquals(ab, guidB, lastModifiedB, deleted, collection);
    recordEquals(ac, guidC, lastModifiedC, deleted, collection);
    recordEquals(ba, guidA, lastModifiedA, deleted, collection);
    recordEquals(bb, guidB, lastModifiedB, deleted, collection);
    recordEquals(bc, guidC, lastModifiedC, deleted, collection);
    recordEquals(aa, ba);
    recordEquals(ab, bb);
    recordEquals(ac, bc);
  }

  public abstract class SuccessfulSynchronizerDelegate implements SynchronizerDelegate {
    public long syncAOne = 0;
    public long syncBOne = 0;

    @Override
    public void onSynchronizeFailed(Synchronizer synchronizer,
                                    Exception lastException, String reason) {
      fail("Should not fail.");
    }
  }

  @Test
  public void testSynchronizerPersists() {
    final Object monitor = new Object();
    final long earliest = new Date().getTime();

    Context context = null;
    final WBORepository repoA = new WBORepository();
    final WBORepository repoB = new WBORepository();
    Synchronizer synchronizer = new Synchronizer();
    synchronizer.bundleA     = new RepositorySessionBundle(0);
    synchronizer.bundleB     = new RepositorySessionBundle(0);
    synchronizer.repositoryA = repoA;
    synchronizer.repositoryB = repoB;

    final SuccessfulSynchronizerDelegate delegateOne = new SuccessfulSynchronizerDelegate() {
      @Override
      public void onSynchronized(Synchronizer synchronizer) {
        Logger.trace(LOG_TAG, "onSynchronized. Success!");
        syncAOne = synchronizer.bundleA.getTimestamp();
        syncBOne = synchronizer.bundleB.getTimestamp();
        synchronized (monitor) {
          monitor.notify();
        }
      }
    };
    final SuccessfulSynchronizerDelegate delegateTwo = new SuccessfulSynchronizerDelegate() {
      @Override
      public void onSynchronized(Synchronizer synchronizer) {
        Logger.trace(LOG_TAG, "onSynchronized. Success!");
        syncAOne = synchronizer.bundleA.getTimestamp();
        syncBOne = synchronizer.bundleB.getTimestamp();
        synchronized (monitor) {
          monitor.notify();
        }
      }
    };
    synchronized (monitor) {
      synchronizer.synchronize(context, delegateOne);
      try {
        monitor.wait();
      } catch (InterruptedException e) {
        fail("Interrupted.");
      }
    }
    long now = new Date().getTime();
    Logger.trace(LOG_TAG, "Earliest is " + earliest);
    Logger.trace(LOG_TAG, "syncAOne is " + delegateOne.syncAOne);
    Logger.trace(LOG_TAG, "syncBOne is " + delegateOne.syncBOne);
    Logger.trace(LOG_TAG, "Now: " + now);
    assertInRangeInclusive(earliest, delegateOne.syncAOne, now);
    assertInRangeInclusive(earliest, delegateOne.syncBOne, now);
    try {
      Thread.sleep(10);
    } catch (InterruptedException e) {
      fail("Thread interrupted!");
    }
    synchronized (monitor) {
      synchronizer.synchronize(context, delegateTwo);
      try {
        monitor.wait();
      } catch (InterruptedException e) {
        fail("Interrupted.");
      }
    }
    now = new Date().getTime();
    Logger.trace(LOG_TAG, "Earliest is " + earliest);
    Logger.trace(LOG_TAG, "syncAOne is " + delegateTwo.syncAOne);
    Logger.trace(LOG_TAG, "syncBOne is " + delegateTwo.syncBOne);
    Logger.trace(LOG_TAG, "Now: " + now);
    assertInRangeInclusive(earliest, delegateTwo.syncAOne, now);
    assertInRangeInclusive(earliest, delegateTwo.syncBOne, now);
    assertTrue(delegateTwo.syncAOne > delegateOne.syncAOne);
    assertTrue(delegateTwo.syncBOne > delegateOne.syncBOne);
    Logger.trace(LOG_TAG, "Reached end of test.");
  }

  private Synchronizer getTestSynchronizer(long tsA, long tsB) {
    WBORepository repoA = new TrackingWBORepository();
    WBORepository repoB = new TrackingWBORepository();
    Synchronizer synchronizer = new Synchronizer();
    synchronizer.bundleA      = new RepositorySessionBundle(tsA);
    synchronizer.bundleB      = new RepositorySessionBundle(tsB);
    synchronizer.repositoryA  = repoA;
    synchronizer.repositoryB  = repoB;
    return synchronizer;
  }

  /**
   * Let's put data in two repos and synchronize them with last sync
   * timestamps later than all of the records. Verify that no records
   * are exchanged.
   */
  @Test
  public void testSynchronizerFakeTimestamps() {
    final Context context = null;

    final String collection  = "bookmarks";
    final boolean deleted    = false;
    final String guidA       = "abcdabcdabcd";
    final String guidB       = "ffffffffffff";
    final long lastModifiedA = 312345;
    final long lastModifiedB = 412345;
    BookmarkRecord bookmarkRecordA = new BookmarkRecord(guidA, collection, lastModifiedA, deleted);
    BookmarkRecord bookmarkRecordB = new BookmarkRecord(guidB, collection, lastModifiedB, deleted);

    final Synchronizer synchronizer = getTestSynchronizer(lastModifiedA + 10, lastModifiedB + 10);
    final WBORepository repoA = (WBORepository) synchronizer.repositoryA;
    final WBORepository repoB = (WBORepository) synchronizer.repositoryB;

    repoA.wbos.put(guidA, bookmarkRecordA);
    repoB.wbos.put(guidB, bookmarkRecordB);

    WaitHelper.getTestWaiter().performWait(new Runnable() {
      @Override
      public void run() {
        synchronizer.synchronize(context, new SynchronizerDelegate() {

          @Override
          public void onSynchronized(Synchronizer synchronizer) {
            try {
              // No records get sent either way.
              final SynchronizerSession synchronizerSession = synchronizer.getSynchronizerSession();
              assertNotNull(synchronizerSession);
              assertEquals(0, synchronizerSession.getInboundCount());
              assertEquals(0, synchronizerSession.getOutboundCount());
              WaitHelper.getTestWaiter().performNotify();
            } catch (Throwable e) {
              WaitHelper.getTestWaiter().performNotify(e);
            }
          }

          @Override
          public void onSynchronizeFailed(Synchronizer synchronizer,
              Exception lastException, String reason) {
            WaitHelper.getTestWaiter().performNotify(lastException);
          }
        });
      }
    });

    // Verify contents.
    assertTrue(repoA.wbos.containsKey(guidA));
    assertTrue(repoB.wbos.containsKey(guidB));
    assertFalse(repoB.wbos.containsKey(guidA));
    assertFalse(repoA.wbos.containsKey(guidB));
    BookmarkRecord aa = (BookmarkRecord) repoA.wbos.get(guidA);
    BookmarkRecord ab = (BookmarkRecord) repoA.wbos.get(guidB);
    BookmarkRecord ba = (BookmarkRecord) repoB.wbos.get(guidA);
    BookmarkRecord bb = (BookmarkRecord) repoB.wbos.get(guidB);
    assertNull(ab);
    assertNull(ba);
    recordEquals(aa, guidA, lastModifiedA, deleted, collection);
    recordEquals(bb, guidB, lastModifiedB, deleted, collection);
  }


  @Test
  public void testSynchronizer() {
    final Context context = null;

    final String collection = "bookmarks";
    final boolean deleted = false;
    final String guidA = "abcdabcdabcd";
    final String guidB = "ffffffffffff";
    final String guidC = "gggggggggggg";
    final long lastModifiedA = 312345;
    final long lastModifiedB = 412340;
    final long lastModifiedC = 412345;
    BookmarkRecord bookmarkRecordA = new BookmarkRecord(guidA, collection, lastModifiedA, deleted);
    BookmarkRecord bookmarkRecordB = new BookmarkRecord(guidB, collection, lastModifiedB, deleted);
    BookmarkRecord bookmarkRecordC = new BookmarkRecord(guidC, collection, lastModifiedC, deleted);

    final Synchronizer synchronizer = getTestSynchronizer(0, 0);
    final WBORepository repoA = (WBORepository) synchronizer.repositoryA;
    final WBORepository repoB = (WBORepository) synchronizer.repositoryB;

    repoA.wbos.put(guidA, bookmarkRecordA);
    repoB.wbos.put(guidB, bookmarkRecordB);
    repoB.wbos.put(guidC, bookmarkRecordC);

    WaitHelper.getTestWaiter().performWait(new Runnable() {
      @Override
      public void run() {
        synchronizer.synchronize(context, new SynchronizerDelegate() {

          @Override
          public void onSynchronized(Synchronizer synchronizer) {
            try {
              // No records get sent either way.
              final SynchronizerSession synchronizerSession = synchronizer.getSynchronizerSession();
              assertNotNull(synchronizerSession);
              assertEquals(1, synchronizerSession.getInboundCount());
              assertEquals(2, synchronizerSession.getOutboundCount());
              WaitHelper.getTestWaiter().performNotify();
            } catch (Throwable e) {
              WaitHelper.getTestWaiter().performNotify(e);
            }
          }

          @Override
          public void onSynchronizeFailed(Synchronizer synchronizer,
              Exception lastException, String reason) {
            WaitHelper.getTestWaiter().performNotify(lastException);
          }
        });
      }
    });

    // Verify contents.
    assertTrue(repoA.wbos.containsKey(guidA));
    assertTrue(repoA.wbos.containsKey(guidB));
    assertTrue(repoA.wbos.containsKey(guidC));
    assertTrue(repoB.wbos.containsKey(guidA));
    assertTrue(repoB.wbos.containsKey(guidB));
    assertTrue(repoB.wbos.containsKey(guidC));
    BookmarkRecord aa = (BookmarkRecord) repoA.wbos.get(guidA);
    BookmarkRecord ab = (BookmarkRecord) repoA.wbos.get(guidB);
    BookmarkRecord ac = (BookmarkRecord) repoA.wbos.get(guidC);
    BookmarkRecord ba = (BookmarkRecord) repoB.wbos.get(guidA);
    BookmarkRecord bb = (BookmarkRecord) repoB.wbos.get(guidB);
    BookmarkRecord bc = (BookmarkRecord) repoB.wbos.get(guidC);
    recordEquals(aa, guidA, lastModifiedA, deleted, collection);
    recordEquals(ab, guidB, lastModifiedB, deleted, collection);
    recordEquals(ac, guidC, lastModifiedC, deleted, collection);
    recordEquals(ba, guidA, lastModifiedA, deleted, collection);
    recordEquals(bb, guidB, lastModifiedB, deleted, collection);
    recordEquals(bc, guidC, lastModifiedC, deleted, collection);
    recordEquals(aa, ba);
    recordEquals(ab, bb);
    recordEquals(ac, bc);
  }

  static Exception doSynchronize(final Synchronizer synchronizer) {
    final ArrayList<Exception> a = new ArrayList<Exception>();

    WaitHelper.getTestWaiter().performWait(new Runnable() {
      @Override
      public void run() {
        synchronizer.synchronize(null, new SynchronizerDelegate() {
          @Override
          public void onSynchronized(Synchronizer synchronizer) {
            Logger.trace(LOG_TAG, "Got onSynchronized.");
            a.add(null);
            WaitHelper.getTestWaiter().performNotify();
          }

          @Override
          public void onSynchronizeFailed(Synchronizer synchronizer, Exception lastException, String reason) {
            Logger.trace(LOG_TAG, "Got onSynchronizedFailed.");
            a.add(lastException);
            WaitHelper.getTestWaiter().performNotify();
          }
        });
      }
    });

    assertEquals(1, a.size()); // Should not be called multiple times!
    return a.get(0);
  }

  private Synchronizer getSynchronizer(WBORepository remote, WBORepository local) {
    BookmarkRecord[] inbounds = new BookmarkRecord[] {
            new BookmarkRecord("inboundSucc1", "bookmarks", 1, false),
            new BookmarkRecord("inboundSucc2", "bookmarks", 1, false),
            new BookmarkRecord("inboundFail1", "bookmarks", 1, false),
            new BookmarkRecord("inboundSucc3", "bookmarks", 1, false),
            new BookmarkRecord("inboundFail2", "bookmarks", 1, false),
            new BookmarkRecord("inboundFail3", "bookmarks", 1, false),
    };
    BookmarkRecord[] outbounds = new BookmarkRecord[] {
            new BookmarkRecord("outboundFail1", "bookmarks", 1, false),
            new BookmarkRecord("outboundFail2", "bookmarks", 1, false),
            new BookmarkRecord("outboundFail3", "bookmarks", 1, false),
            new BookmarkRecord("outboundFail4", "bookmarks", 1, false),
            new BookmarkRecord("outboundFail5", "bookmarks", 1, false),
            new BookmarkRecord("outboundFail6", "bookmarks", 1, false),
    };
    for (BookmarkRecord inbound : inbounds) {
      remote.wbos.put(inbound.guid, inbound);
    }
    for (BookmarkRecord outbound : outbounds) {
      local.wbos.put(outbound.guid, outbound);
    }

    final Synchronizer synchronizer = new Synchronizer();
    synchronizer.repositoryA = remote;
    synchronizer.repositoryB = local;
    return synchronizer;
  }

  @Test
  public void testNoErrors() {
    WBORepository remote = new TrackingWBORepository();
    WBORepository local  = new TrackingWBORepository();

    Synchronizer synchronizer = getSynchronizer(remote, local);
    assertNull(doSynchronize(synchronizer));

    assertEquals(12, local.wbos.size());
    assertEquals(12, remote.wbos.size());
  }

  @Test
  public void testLocalFetchErrors() {
    WBORepository remote = new TrackingWBORepository();
    WBORepository local  = new SynchronizerHelpers.FailFetchWBORepository(SynchronizerHelpers.FailMode.FETCH);

    Synchronizer synchronizer = getSynchronizer(remote, local);
    Exception e = doSynchronize(synchronizer);
    assertNotNull(e);
    assertEquals(FetchFailedException.class, e.getClass());

    // Neither session gets finished successfully, so all records are dropped.
    assertEquals(6, local.wbos.size());
    assertEquals(6, remote.wbos.size());
  }

  @Test
  public void testRemoteFetchErrors() {
    WBORepository remote = new SynchronizerHelpers.FailFetchWBORepository(SynchronizerHelpers.FailMode.FETCH);
    WBORepository local  = new TrackingWBORepository();

    Synchronizer synchronizer = getSynchronizer(remote, local);
    Exception e = doSynchronize(synchronizer);
    assertNotNull(e);
    assertEquals(FetchFailedException.class, e.getClass());

    // Neither session gets finished successfully, so all records are dropped.
    assertEquals(6, local.wbos.size());
    assertEquals(6, remote.wbos.size());
  }

  @Test
  public void testLocalSerialStoreErrorsAreIgnored() {
    WBORepository remote = new TrackingWBORepository();
    WBORepository local  = new SynchronizerHelpers.SerialFailStoreWBORepository(SynchronizerHelpers.FailMode.FETCH);

    Synchronizer synchronizer = getSynchronizer(remote, local);
    assertNull(doSynchronize(synchronizer));

    assertEquals(9,  local.wbos.size());
    assertEquals(12, remote.wbos.size());
  }

  @Test
  public void testLocalBatchStoreErrorsAreIgnored() {
    final int BATCH_SIZE = 3;

    Synchronizer synchronizer = getSynchronizer(new TrackingWBORepository(), new SynchronizerHelpers.BatchFailStoreWBORepository(BATCH_SIZE));

    Exception e = doSynchronize(synchronizer);
    assertNull(e);
  }

  @Test
  public void testRemoteSerialStoreErrorsAreNotIgnored() throws Exception {
    Synchronizer synchronizer = getSynchronizer(new SynchronizerHelpers.SerialFailStoreWBORepository(SynchronizerHelpers.FailMode.STORE), new TrackingWBORepository()); // Tracking so we don't send incoming records back.

    Exception e = doSynchronize(synchronizer);
    assertNotNull(e);
    assertEquals(StoreFailedException.class, e.getClass());
  }

  @Test
  public void testRemoteBatchStoreErrorsAreNotIgnoredManyBatches() throws Exception {
    final int BATCH_SIZE = 3;

    Synchronizer synchronizer = getSynchronizer(new SynchronizerHelpers.BatchFailStoreWBORepository(BATCH_SIZE), new TrackingWBORepository()); // Tracking so we don't send incoming records back.

    Exception e = doSynchronize(synchronizer);
    assertNotNull(e);
    assertEquals(StoreFailedException.class, e.getClass());
  }

  @Test
  public void testRemoteBatchStoreErrorsAreNotIgnoredOneBigBatch() throws Exception {
    final int BATCH_SIZE = 20;

    Synchronizer synchronizer = getSynchronizer(new SynchronizerHelpers.BatchFailStoreWBORepository(BATCH_SIZE), new TrackingWBORepository()); // Tracking so we don't send incoming records back.

    Exception e = doSynchronize(synchronizer);
    assertNotNull(e);
    assertEquals(StoreFailedException.class, e.getClass());
  }

  @Test
  public void testSessionRemoteBeginError() {
    Synchronizer synchronizer = getSynchronizer(new SynchronizerHelpers.BeginErrorWBORepository(), new TrackingWBORepository());
    Exception e = doSynchronize(synchronizer);
    assertNotNull(e);
    assertEquals(SynchronizerHelpers.BeginFailedException.class, e.getClass());
  }

  @Test
  public void testSessionLocalBeginError() {
    Synchronizer synchronizer = getSynchronizer(new TrackingWBORepository(), new SynchronizerHelpers.BeginErrorWBORepository());
    Exception e = doSynchronize(synchronizer);
    assertNotNull(e);
    assertEquals(SynchronizerHelpers.BeginFailedException.class, e.getClass());
  }

  @Test
  public void testSessionRemoteFinishError() {
    Synchronizer synchronizer = getSynchronizer(new SynchronizerHelpers.FinishErrorWBORepository(), new TrackingWBORepository());
    Exception e = doSynchronize(synchronizer);
    assertNotNull(e);
    assertEquals(SynchronizerHelpers.FinishFailedException.class, e.getClass());
  }

  @Test
  public void testSessionLocalFinishError() {
    Synchronizer synchronizer = getSynchronizer(new TrackingWBORepository(), new SynchronizerHelpers.FinishErrorWBORepository());
    Exception e = doSynchronize(synchronizer);
    assertNotNull(e);
    assertEquals(SynchronizerHelpers.FinishFailedException.class, e.getClass());
  }

  @Test
  public void testSessionBothBeginError() {
    Synchronizer synchronizer = getSynchronizer(new SynchronizerHelpers.BeginErrorWBORepository(), new SynchronizerHelpers.BeginErrorWBORepository());
    Exception e = doSynchronize(synchronizer);
    assertNotNull(e);
    assertEquals(SynchronizerHelpers.BeginFailedException.class, e.getClass());
  }

  @Test
  public void testSessionBothFinishError() {
    Synchronizer synchronizer = getSynchronizer(new SynchronizerHelpers.FinishErrorWBORepository(), new SynchronizerHelpers.FinishErrorWBORepository());
    Exception e = doSynchronize(synchronizer);
    assertNotNull(e);
    assertEquals(SynchronizerHelpers.FinishFailedException.class, e.getClass());
  }
}
