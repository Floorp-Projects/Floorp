/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.android.sync.test;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.android.sync.test.SynchronizerHelpers.FailFetchWBORepository;
import org.mozilla.android.sync.test.helpers.ExpectSuccessRepositorySessionCreationDelegate;
import org.mozilla.android.sync.test.helpers.ExpectSuccessRepositorySessionFinishDelegate;
import org.mozilla.gecko.background.testhelpers.TestRunner;
import org.mozilla.gecko.background.testhelpers.WBORepository;
import org.mozilla.gecko.background.testhelpers.WaitHelper;
import org.mozilla.gecko.sync.repositories.InactiveSessionException;
import org.mozilla.gecko.sync.repositories.InvalidSessionTransitionException;
import org.mozilla.gecko.sync.repositories.Repository;
import org.mozilla.gecko.sync.repositories.RepositorySession;
import org.mozilla.gecko.sync.repositories.RepositorySessionBundle;
import org.mozilla.gecko.sync.repositories.domain.BookmarkRecord;
import org.mozilla.gecko.sync.synchronizer.RecordsChannel;
import org.mozilla.gecko.sync.synchronizer.RecordsChannelDelegate;

import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicInteger;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

@RunWith(TestRunner.class)
public class TestRecordsChannel {

  protected WBORepository remote;
  protected WBORepository local;

  protected RepositorySession source;
  protected RepositorySession sink;
  protected RecordsChannelDelegate rcDelegate;

  protected AtomicInteger numFlowFetchFailed;
  protected AtomicInteger numFlowStoreFailed;
  protected AtomicInteger numFlowCompleted;
  protected AtomicBoolean flowBeginFailed;
  protected AtomicBoolean flowFinishFailed;

  public void doFlow(final Repository remote, final Repository local) throws Exception {
    WaitHelper.getTestWaiter().performWait(new Runnable() {
      @Override
      public void run() {
        remote.createSession(new ExpectSuccessRepositorySessionCreationDelegate(WaitHelper.getTestWaiter()) {
          @Override
          public void onSessionCreated(RepositorySession session) {
            source = session;
            local.createSession(new ExpectSuccessRepositorySessionCreationDelegate(WaitHelper.getTestWaiter()) {
              @Override
              public void onSessionCreated(RepositorySession session) {
                sink = session;
                WaitHelper.getTestWaiter().performNotify();
              }
            }, null);
          }
        }, null);
      }
    });

    assertNotNull(source);
    assertNotNull(sink);

    numFlowFetchFailed = new AtomicInteger(0);
    numFlowStoreFailed = new AtomicInteger(0);
    numFlowCompleted = new AtomicInteger(0);
    flowBeginFailed = new AtomicBoolean(false);
    flowFinishFailed = new AtomicBoolean(false);

    rcDelegate = new RecordsChannelDelegate() {
      @Override
      public void onFlowFetchFailed(RecordsChannel recordsChannel, Exception ex) {
        numFlowFetchFailed.incrementAndGet();
      }

      @Override
      public void onFlowStoreFailed(RecordsChannel recordsChannel, Exception ex, String recordGuid) {
        numFlowStoreFailed.incrementAndGet();
      }

      @Override
      public void onFlowFinishFailed(RecordsChannel recordsChannel, Exception ex) {
        flowFinishFailed.set(true);
        WaitHelper.getTestWaiter().performNotify();
      }

      @Override
      public void onFlowCompleted(RecordsChannel recordsChannel, long fetchEnd, long storeEnd) {
        numFlowCompleted.incrementAndGet();
        try {
          sink.finish(new ExpectSuccessRepositorySessionFinishDelegate(WaitHelper.getTestWaiter()) {
            @Override
            public void onFinishSucceeded(RepositorySession session, RepositorySessionBundle bundle) {
              try {
                source.finish(new ExpectSuccessRepositorySessionFinishDelegate(WaitHelper.getTestWaiter()) {
                  @Override
                  public void onFinishSucceeded(RepositorySession session, RepositorySessionBundle bundle) {
                    performNotify();
                  }
                });
              } catch (InactiveSessionException e) {
                WaitHelper.getTestWaiter().performNotify(e);
              }
            }
          });
        } catch (InactiveSessionException e) {
          WaitHelper.getTestWaiter().performNotify(e);
        }
      }

      @Override
      public void onFlowBeginFailed(RecordsChannel recordsChannel, Exception ex) {
        flowBeginFailed.set(true);
        WaitHelper.getTestWaiter().performNotify();
      }
    };

    final RecordsChannel rc = new RecordsChannel(source,  sink, rcDelegate);
    WaitHelper.getTestWaiter().performWait(new Runnable() {
      @Override
      public void run() {
        try {
          rc.beginAndFlow();
        } catch (InvalidSessionTransitionException e) {
          WaitHelper.getTestWaiter().performNotify(e);
        }
      }
    });
  }

  public static final BookmarkRecord[] inbounds = new BookmarkRecord[] {
    new BookmarkRecord("inboundSucc1", "bookmarks", 1, false),
    new BookmarkRecord("inboundSucc2", "bookmarks", 1, false),
    new BookmarkRecord("inboundFail1", "bookmarks", 1, false),
    new BookmarkRecord("inboundSucc3", "bookmarks", 1, false),
    new BookmarkRecord("inboundSucc4", "bookmarks", 1, false),
    new BookmarkRecord("inboundFail2", "bookmarks", 1, false),
  };
  public static final BookmarkRecord[] outbounds = new BookmarkRecord[] {
      new BookmarkRecord("outboundSucc1", "bookmarks", 1, false),
      new BookmarkRecord("outboundSucc2", "bookmarks", 1, false),
      new BookmarkRecord("outboundSucc3", "bookmarks", 1, false),
      new BookmarkRecord("outboundSucc4", "bookmarks", 1, false),
      new BookmarkRecord("outboundSucc5", "bookmarks", 1, false),
      new BookmarkRecord("outboundFail6", "bookmarks", 1, false),
  };

  protected WBORepository empty() {
    WBORepository repo = new SynchronizerHelpers.TrackingWBORepository();
    return repo;
  }

  protected WBORepository full() {
    WBORepository repo = new SynchronizerHelpers.TrackingWBORepository();
    for (BookmarkRecord outbound : outbounds) {
      repo.wbos.put(outbound.guid, outbound);
    }
    return repo;
  }

  protected WBORepository failingFetch() {
    WBORepository repo = new FailFetchWBORepository();
    for (BookmarkRecord outbound : outbounds) {
      repo.wbos.put(outbound.guid, outbound);
    }
    return repo;
  }

  @Test
  public void testSuccess() throws Exception {
    WBORepository source = full();
    WBORepository sink = empty();
    doFlow(source, sink);
    assertEquals(1, numFlowCompleted.get());
    assertEquals(0, numFlowFetchFailed.get());
    assertEquals(0, numFlowStoreFailed.get());
    assertEquals(source.wbos, sink.wbos);
  }

  @Test
  public void testFetchFail() throws Exception {
    WBORepository source = failingFetch();
    WBORepository sink = empty();
    doFlow(source, sink);
    assertEquals(1, numFlowCompleted.get());
    assertTrue(numFlowFetchFailed.get() > 0);
    assertEquals(0, numFlowStoreFailed.get());
    assertTrue(sink.wbos.size() < 6);
  }

  @Test
  public void testStoreSerialFail() throws Exception {
    WBORepository source = full();
    WBORepository sink = new SynchronizerHelpers.SerialFailStoreWBORepository();
    doFlow(source, sink);
    assertEquals(1, numFlowCompleted.get());
    assertEquals(0, numFlowFetchFailed.get());
    assertEquals(1, numFlowStoreFailed.get());
    assertEquals(5, sink.wbos.size());
  }

  @Test
  public void testStoreBatchesFail() throws Exception {
    WBORepository source = full();
    WBORepository sink = new SynchronizerHelpers.BatchFailStoreWBORepository(3);
    doFlow(source, sink);
    assertEquals(1, numFlowCompleted.get());
    assertEquals(0, numFlowFetchFailed.get());
    assertEquals(3, numFlowStoreFailed.get()); // One batch fails.
    assertEquals(3, sink.wbos.size()); // One batch succeeds.
  }


  @Test
  public void testStoreOneBigBatchFail() throws Exception {
    WBORepository source = full();
    WBORepository sink = new SynchronizerHelpers.BatchFailStoreWBORepository(50);
    doFlow(source, sink);
    assertEquals(1, numFlowCompleted.get());
    assertEquals(0, numFlowFetchFailed.get());
    assertEquals(6, numFlowStoreFailed.get()); // One (big) batch fails.
    assertEquals(0, sink.wbos.size()); // No batches succeed.
  }
}
