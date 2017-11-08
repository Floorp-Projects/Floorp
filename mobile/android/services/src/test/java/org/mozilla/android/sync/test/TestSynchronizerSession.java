/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.android.sync.test;

import android.content.Context;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.android.sync.test.SynchronizerHelpers.DataAvailableWBORepository;
import org.mozilla.android.sync.test.SynchronizerHelpers.ShouldSkipWBORepository;
import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.background.testhelpers.TestRunner;
import org.mozilla.gecko.background.testhelpers.WBORepository;
import org.mozilla.gecko.background.testhelpers.WaitHelper;
import org.mozilla.gecko.sync.SynchronizerConfiguration;
import org.mozilla.gecko.sync.repositories.RepositorySessionBundle;
import org.mozilla.gecko.sync.repositories.domain.BookmarkRecord;
import org.mozilla.gecko.sync.repositories.domain.Record;
import org.mozilla.gecko.sync.synchronizer.Synchronizer;
import org.mozilla.gecko.sync.synchronizer.SynchronizerSession;
import org.mozilla.gecko.sync.synchronizer.SynchronizerSessionDelegate;

import java.util.HashMap;
import java.util.Map;
import java.util.Map.Entry;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

@RunWith(TestRunner.class)
public class TestSynchronizerSession {
  public static final String LOG_TAG = TestSynchronizerSession.class.getSimpleName();

  protected static void assertFirstContainsSecond(Map<String, Record> first, Map<String, Record> second) {
    for (Entry<String, Record> entry : second.entrySet()) {
      assertTrue("Expected key " + entry.getKey(), first.containsKey(entry.getKey()));
      Record record = first.get(entry.getKey());
      assertEquals(entry.getValue(), record);
    }
  }

  protected static void assertFirstDoesNotContainSecond(Map<String, Record> first, Map<String, Record> second) {
    for (Entry<String, Record> entry : second.entrySet()) {
      assertFalse("Unexpected key " + entry.getKey(), first.containsKey(entry.getKey()));
    }
  }

  protected WBORepository repoA = null;
  protected WBORepository repoB = null;
  protected SynchronizerSession syncSession = null;
  protected Map<String, Record> originalWbosA = null;
  protected Map<String, Record> originalWbosB = null;

  @Before
  public void setUp() {
    repoA = new DataAvailableWBORepository(false);
    repoB = new DataAvailableWBORepository(false);

    final String collection  = "bookmarks";
    final boolean deleted    = false;
    final String guidA       = "abcdabcdabcd";
    final String guidB       = "ffffffffffff";
    final String guidC       = "xxxxxxxxxxxx";
    final long lastModifiedA = 312345;
    final long lastModifiedB = 412340;
    final long lastModifiedC = 412345;
    final BookmarkRecord bookmarkRecordA = new BookmarkRecord(guidA, collection, lastModifiedA, deleted);
    final BookmarkRecord bookmarkRecordB = new BookmarkRecord(guidB, collection, lastModifiedB, deleted);
    final BookmarkRecord bookmarkRecordC = new BookmarkRecord(guidC, collection, lastModifiedC, deleted);

    repoA.wbos.put(guidA, bookmarkRecordA);
    repoB.wbos.put(guidB, bookmarkRecordB);
    repoB.wbos.put(guidC, bookmarkRecordC);

    originalWbosA = new HashMap<String, Record>(repoA.wbos);
    originalWbosB = new HashMap<String, Record>(repoB.wbos);

    Synchronizer synchronizer = new Synchronizer();
    synchronizer.repositoryA = repoA;
    synchronizer.repositoryB = repoB;
    syncSession = new SynchronizerSession(synchronizer, new SynchronizerSessionDelegate() {
      @Override
      public void onInitialized(SynchronizerSession session) {
        session.synchronize();
      }

      @Override
      public void onSynchronized(SynchronizerSession session) {
        WaitHelper.getTestWaiter().performNotify();
      }

      @Override
      public void onSynchronizeFailed(SynchronizerSession session, Exception lastException, String reason) {
        WaitHelper.getTestWaiter().performNotify(lastException);
      }

      @Override
      public void onSynchronizeSkipped(SynchronizerSession synchronizerSession) {
        WaitHelper.getTestWaiter().performNotify(new RuntimeException("Not expecting onSynchronizeSkipped"));
      }
    });
  }

  protected void logStats() {
    // Uncomment this line to print stats to console:
    // Logger.startLoggingTo(new PrintLogWriter(new PrintWriter(System.out, true)));

    Logger.debug(LOG_TAG, "Repo A fetch done: " + repoA.stats.fetchCompleted);
    Logger.debug(LOG_TAG, "Repo B store done: " + repoB.stats.storeCompleted);
    Logger.debug(LOG_TAG, "Repo B fetch done: " + repoB.stats.fetchCompleted);
    Logger.debug(LOG_TAG, "Repo A store done: " + repoA.stats.storeCompleted);

    SynchronizerConfiguration sc = syncSession.getSynchronizer().save();
    Logger.debug(LOG_TAG, "Repo A timestamp: " + sc.remoteBundle.getTimestamp());
    Logger.debug(LOG_TAG, "Repo B timestamp: " + sc.localBundle.getTimestamp());
  }

  protected void doTest(boolean remoteDataAvailable, boolean localDataAvailable) {
    ((DataAvailableWBORepository) repoA).dataAvailable = remoteDataAvailable;
    ((DataAvailableWBORepository) repoB).dataAvailable = localDataAvailable;

    WaitHelper.getTestWaiter().performWait(new Runnable() {
      @Override
      public void run() {
        final Context context = null;
        syncSession.init(context,
            new RepositorySessionBundle(0),
            new RepositorySessionBundle(0));
      }
    });

    logStats();
  }

  @Test
  public void testSynchronizerSessionBothHaveData() {
    long before = System.currentTimeMillis();
    boolean remoteDataAvailable = true;
    boolean localDataAvailable = true;
    doTest(remoteDataAvailable, localDataAvailable);
    long after = System.currentTimeMillis();

    assertEquals(1, syncSession.getInboundCount());
    assertEquals(2, syncSession.getOutboundCount());

    // Didn't lose any records.
    assertFirstContainsSecond(repoA.wbos, originalWbosA);
    assertFirstContainsSecond(repoB.wbos, originalWbosB);
    // Got new records.
    assertFirstContainsSecond(repoA.wbos, originalWbosB);
    assertFirstContainsSecond(repoB.wbos, originalWbosA);

    // Timestamps updated.
    SynchronizerConfiguration sc = syncSession.getSynchronizer().save();
    TestSynchronizer.assertInRangeInclusive(before, sc.localBundle.getTimestamp(), after);
    TestSynchronizer.assertInRangeInclusive(before, sc.remoteBundle.getTimestamp(), after);
  }

  @Test
  public void testSynchronizerSessionOnlyLocalHasData() {
    long before = System.currentTimeMillis();
    boolean remoteDataAvailable = false;
    boolean localDataAvailable = true;
    doTest(remoteDataAvailable, localDataAvailable);
    long after = System.currentTimeMillis();

    // Record counts updated.
    assertEquals(0, syncSession.getInboundCount());
    assertEquals(2, syncSession.getOutboundCount());

    // Didn't lose any records.
    assertFirstContainsSecond(repoA.wbos, originalWbosA);
    assertFirstContainsSecond(repoB.wbos, originalWbosB);
    // Got new records.
    assertFirstContainsSecond(repoA.wbos, originalWbosB);
    // Didn't get records we shouldn't have fetched.
    assertFirstDoesNotContainSecond(repoB.wbos, originalWbosA);

    // Timestamps updated.
    SynchronizerConfiguration sc = syncSession.getSynchronizer().save();
    TestSynchronizer.assertInRangeInclusive(before, sc.localBundle.getTimestamp(), after);
    TestSynchronizer.assertInRangeInclusive(before, sc.remoteBundle.getTimestamp(), after);
  }

  @Test
  public void testSynchronizerSessionOnlyRemoteHasData() {
    long before = System.currentTimeMillis();
    boolean remoteDataAvailable = true;
    boolean localDataAvailable = false;
    doTest(remoteDataAvailable, localDataAvailable);
    long after = System.currentTimeMillis();

    // Record counts updated.
    assertEquals(1, syncSession.getInboundCount());
    assertEquals(0, syncSession.getOutboundCount());

    // Didn't lose any records.
    assertFirstContainsSecond(repoA.wbos, originalWbosA);
    assertFirstContainsSecond(repoB.wbos, originalWbosB);
    // Got new records.
    assertFirstContainsSecond(repoB.wbos, originalWbosA);
    // Didn't get records we shouldn't have fetched.
    assertFirstDoesNotContainSecond(repoA.wbos, originalWbosB);

    // Timestamps updated.
    SynchronizerConfiguration sc = syncSession.getSynchronizer().save();
    TestSynchronizer.assertInRangeInclusive(before, sc.localBundle.getTimestamp(), after);
    TestSynchronizer.assertInRangeInclusive(before, sc.remoteBundle.getTimestamp(), after);
  }

  @Test
  public void testSynchronizerSessionNeitherHaveData() {
    long before = System.currentTimeMillis();
    boolean remoteDataAvailable = false;
    boolean localDataAvailable = false;
    doTest(remoteDataAvailable, localDataAvailable);
    long after = System.currentTimeMillis();

    // Record counts updated.
    assertEquals(0, syncSession.getInboundCount());
    assertEquals(0, syncSession.getOutboundCount());

    // Didn't lose any records.
    assertFirstContainsSecond(repoA.wbos, originalWbosA);
    assertFirstContainsSecond(repoB.wbos, originalWbosB);
    // Didn't get records we shouldn't have fetched.
    assertFirstDoesNotContainSecond(repoA.wbos, originalWbosB);
    assertFirstDoesNotContainSecond(repoB.wbos, originalWbosA);

    // Timestamps not updated.
    SynchronizerConfiguration sc = syncSession.getSynchronizer().save();
    assertEquals(0L, sc.localBundle.getTimestamp());
    assertEquals(0L, sc.remoteBundle.getTimestamp());
  }

  protected void doSkipTest(boolean remoteShouldSkip, boolean localShouldSkip) {
    repoA = new ShouldSkipWBORepository(remoteShouldSkip);
    repoB = new ShouldSkipWBORepository(localShouldSkip);

    Synchronizer synchronizer = new Synchronizer();
    synchronizer.repositoryA = repoA;
    synchronizer.repositoryB = repoB;

    syncSession = new SynchronizerSession(synchronizer, new SynchronizerSessionDelegate() {
      @Override
      public void onInitialized(SynchronizerSession session) {
        session.synchronize();
      }

      @Override
      public void onSynchronized(SynchronizerSession session) {
        WaitHelper.getTestWaiter().performNotify(new RuntimeException("Not expecting onSynchronized"));
      }

      @Override
      public void onSynchronizeFailed(SynchronizerSession session, Exception lastException, String reason) {
        WaitHelper.getTestWaiter().performNotify(lastException);
      }

      @Override
      public void onSynchronizeSkipped(SynchronizerSession synchronizerSession) {
        WaitHelper.getTestWaiter().performNotify();
      }
    });

    WaitHelper.getTestWaiter().performWait(new Runnable() {
      @Override
      public void run() {
        final Context context = null;
        syncSession.init(context,
            new RepositorySessionBundle(100),
            new RepositorySessionBundle(200));
      }
    });

    // If we skip, we don't update timestamps or even un-bundle.
    SynchronizerConfiguration sc = syncSession.getSynchronizer().save();
    assertNotNull(sc);
    assertNull(sc.localBundle);
    assertNull(sc.remoteBundle);
    assertEquals(-1, syncSession.getInboundCount());
    assertEquals(-1, syncSession.getOutboundCount());
  }

  @Test
  public void testSynchronizerSessionShouldSkip() {
    // These combinations should all skip.
    doSkipTest(true, false);

    doSkipTest(false, true);
    doSkipTest(true, true);

    try {
      doSkipTest(false, false);
      fail("Expected exception.");
    } catch (WaitHelper.InnerError e) {
      assertTrue(e.innerError instanceof RuntimeException);
      assertEquals("Not expecting onSynchronized", e.innerError.getMessage());
    }
  }
}
