/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.background.sync;

import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicLong;

import junit.framework.AssertionFailedError;

import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.background.helpers.AndroidSyncTestCase;
import org.mozilla.gecko.background.sync.helpers.SimpleSuccessBeginDelegate;
import org.mozilla.gecko.background.sync.helpers.SimpleSuccessCreationDelegate;
import org.mozilla.gecko.background.sync.helpers.SimpleSuccessFetchDelegate;
import org.mozilla.gecko.background.sync.helpers.SimpleSuccessFinishDelegate;
import org.mozilla.gecko.background.sync.helpers.SimpleSuccessStoreDelegate;
import org.mozilla.gecko.background.testhelpers.WBORepository;
import org.mozilla.gecko.sync.CryptoRecord;
import org.mozilla.gecko.sync.ExtendedJSONObject;
import org.mozilla.gecko.sync.repositories.InactiveSessionException;
import org.mozilla.gecko.sync.repositories.InvalidSessionTransitionException;
import org.mozilla.gecko.sync.repositories.NoStoreDelegateException;
import org.mozilla.gecko.sync.repositories.RepositorySession;
import org.mozilla.gecko.sync.repositories.RepositorySessionBundle;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionCreationDelegate;
import org.mozilla.gecko.sync.repositories.domain.BookmarkRecord;
import org.mozilla.gecko.sync.repositories.domain.Record;
import org.mozilla.gecko.sync.synchronizer.Synchronizer;
import org.mozilla.gecko.sync.synchronizer.SynchronizerDelegate;

import android.content.Context;

public class TestStoreTracking extends AndroidSyncTestCase {
  public void assertEq(Object expected, Object actual) {
    try {
      assertEquals(expected, actual);
    } catch (AssertionFailedError e) {
      performNotify(e);
    }
  }

  public class TrackingWBORepository extends WBORepository {
    @Override
    public synchronized boolean shouldTrack() {
      return true;
    }
  }

  public void doTestStoreRetrieveByGUID(final WBORepository repository,
                                        final RepositorySession session,
                                        final String expectedGUID,
                                        final Record record) {

    final SimpleSuccessStoreDelegate storeDelegate = new SimpleSuccessStoreDelegate() {

      @Override
      public void onRecordStoreSucceeded(String guid) {
        Logger.debug(getName(), "Stored " + guid);
        assertEq(expectedGUID, guid);
      }

      @Override
      public void onStoreCompleted(long storeEnd) {
        Logger.debug(getName(), "Store completed at " + storeEnd + ".");
        try {
          session.fetch(new String[] { expectedGUID }, new SimpleSuccessFetchDelegate() {
            @Override
            public void onFetchedRecord(Record record) {
              Logger.debug(getName(), "Hurrah! Fetched record " + record.guid);
              assertEq(expectedGUID, record.guid);
            }

            @Override
            public void onFetchCompleted(final long fetchEnd) {
              Logger.debug(getName(), "Fetch completed at " + fetchEnd + ".");

              // But fetching by time returns nothing.
              session.fetchSince(0, new SimpleSuccessFetchDelegate() {
                private AtomicBoolean fetched = new AtomicBoolean(false);

                @Override
                public void onFetchedRecord(Record record) {
                  Logger.debug(getName(), "Fetched record " + record.guid);
                  fetched.set(true);
                  performNotify(new AssertionFailedError("Should have fetched no record!"));
                }

                @Override
                public void onFetchCompleted(final long fetchEnd) {
                  if (fetched.get()) {
                    Logger.debug(getName(), "Not finishing session: record retrieved.");
                    return;
                  }
                  try {
                    session.finish(new SimpleSuccessFinishDelegate() {
                      @Override
                      public void onFinishSucceeded(RepositorySession session,
                                                    RepositorySessionBundle bundle) {
                        performNotify();
                      }
                    });
                  } catch (InactiveSessionException e) {
                    performNotify(e);
                  }
                }

                @Override
                public void onBatchCompleted() {

                }
              });
            }

            @Override
            public void onBatchCompleted() {

            }
          });
        } catch (InactiveSessionException e) {
          performNotify(e);
        }
      }
    };

    session.setStoreDelegate(storeDelegate);
    try {
      Logger.debug(getName(), "Storing...");
      session.store(record);
      session.storeDone();
    } catch (NoStoreDelegateException e) {
      // Should not happen.
    }
  }

  private void doTestNewSessionRetrieveByTime(final WBORepository repository,
                                              final String expectedGUID) {
    final SimpleSuccessCreationDelegate createDelegate = new SimpleSuccessCreationDelegate() {
      @Override
      public void onSessionCreated(final RepositorySession session) {
        Logger.debug(getName(), "Session created.");
        try {
          session.begin(new SimpleSuccessBeginDelegate() {
            @Override
            public void onBeginSucceeded(final RepositorySession session) {
              // Now we get a result.
              session.fetchSince(0, new SimpleSuccessFetchDelegate() {

                @Override
                public void onFetchedRecord(Record record) {
                  assertEq(expectedGUID, record.guid);
                }

                @Override
                public void onFetchCompleted(long end) {
                  try {
                    session.finish(new SimpleSuccessFinishDelegate() {
                      @Override
                      public void onFinishSucceeded(RepositorySession session,
                                                    RepositorySessionBundle bundle) {
                        // Hooray!
                        performNotify();
                      }
                    });
                  } catch (InactiveSessionException e) {
                    performNotify(e);
                  }
                }

                @Override
                public void onBatchCompleted() {

                }
              });
            }
          });
        } catch (InvalidSessionTransitionException e) {
          performNotify(e);
        }
      }
    };
    Runnable create = new Runnable() {
      @Override
      public void run() {
        repository.createSession(createDelegate, getApplicationContext());
      }
    };

    performWait(create);
  }

  /**
   * Store a record in one session. Verify that fetching by GUID returns
   * the record. Verify that fetching by timestamp fails to return records.
   * Start a new session. Verify that fetching by timestamp returns the
   * stored record.
   *
   * Invokes doTestStoreRetrieveByGUID, doTestNewSessionRetrieveByTime.
   */
  public void testStoreRetrieveByGUID() {
    Logger.debug(getName(), "Started.");
    final WBORepository r = new TrackingWBORepository();
    final long now = System.currentTimeMillis();
    final String expectedGUID = "abcdefghijkl";
    final Record record = new BookmarkRecord(expectedGUID, "bookmarks", now , false);

    final RepositorySessionCreationDelegate createDelegate = new SimpleSuccessCreationDelegate() {
      @Override
      public void onSessionCreated(RepositorySession session) {
        Logger.debug(getName(), "Session created: " + session);
        try {
          session.begin(new SimpleSuccessBeginDelegate() {
            @Override
            public void onBeginSucceeded(final RepositorySession session) {
              doTestStoreRetrieveByGUID(r, session, expectedGUID, record);
            }
          });
        } catch (InvalidSessionTransitionException e) {
          performNotify(e);
        }
      }
    };

    final Context applicationContext = getApplicationContext();

    // This has to happen on a new thread so that we
    // can wait for it!
    Runnable create = onThreadRunnable(new Runnable() {
      @Override
      public void run() {
        r.createSession(createDelegate, applicationContext);
      }
    });

    Runnable retrieve = onThreadRunnable(new Runnable() {
      @Override
      public void run() {
        doTestNewSessionRetrieveByTime(r, expectedGUID);
        performNotify();
      }
    });

    performWait(create);
    performWait(retrieve);
  }

  private Runnable onThreadRunnable(final Runnable r) {
    return new Runnable() {
      @Override
      public void run() {
        new Thread(r).start();
      }
    };
  }


  public class CountingWBORepository extends TrackingWBORepository {
    public AtomicLong counter = new AtomicLong(0L);
    public class CountingWBORepositorySession extends WBORepositorySession {
      private static final String LOG_TAG = "CountingRepoSession";

      public CountingWBORepositorySession(WBORepository repository) {
        super(repository);
      }

      @Override
      public void store(final Record record) throws NoStoreDelegateException {
        Logger.debug(LOG_TAG, "Counter now " + counter.incrementAndGet());
        super.store(record);
      }
    }

    @Override
    public void createSession(RepositorySessionCreationDelegate delegate,
                              Context context) {
      delegate.deferredCreationDelegate().onSessionCreated(new CountingWBORepositorySession(this));
    }
  }

  public class TestRecord extends Record {
    public TestRecord(String guid, String collection, long lastModified,
                      boolean deleted) {
      super(guid, collection, lastModified, deleted);
    }

    @Override
    public void initFromEnvelope(CryptoRecord payload) {
      return;
    }

    @Override
    public CryptoRecord getEnvelope() {
      return null;
    }

    @Override
    protected void populatePayload(ExtendedJSONObject payload) {
    }

    @Override
    protected void initFromPayload(ExtendedJSONObject payload) {
    }

    @Override
    public Record copyWithIDs(String guid, long androidID) {
      return new TestRecord(guid, this.collection, this.lastModified, this.deleted);
    }
  }

  /**
   * Create two repositories, syncing from one to the other. Ensure
   * that records stored from one aren't re-uploaded.
   */
  public void testStoreBetweenRepositories() {
    final CountingWBORepository repoA = new CountingWBORepository();    // "Remote". First source.
    final CountingWBORepository repoB = new CountingWBORepository();    // "Local". First sink.
    long now = System.currentTimeMillis();

    TestRecord recordA1 = new TestRecord("aacdefghiaaa", "coll", now - 30, false);
    TestRecord recordA2 = new TestRecord("aacdefghibbb", "coll", now - 20, false);
    TestRecord recordB1 = new TestRecord("aacdefghiaaa", "coll", now - 10, false);
    TestRecord recordB2 = new TestRecord("aacdefghibbb", "coll", now - 40, false);

    TestRecord recordA3 = new TestRecord("nncdefghibbb", "coll", now, false);
    TestRecord recordB3 = new TestRecord("nncdefghiaaa", "coll", now, false);

    // A1 and B1 are the same, but B's version is newer. We expect A1 to be downloaded
    // and B1 to be uploaded.
    // A2 and B2 are the same, but A's version is newer. We expect A2 to be downloaded
    // and B2 to not be uploaded.
    // Both A3 and B3 are new. We expect them to go in each direction.
    // Expected counts, then:
    // Repo A: B1 + B3
    // Repo B: A1 + A2 + A3
    repoB.wbos.put(recordB1.guid, recordB1);
    repoB.wbos.put(recordB2.guid, recordB2);
    repoB.wbos.put(recordB3.guid, recordB3);
    repoA.wbos.put(recordA1.guid, recordA1);
    repoA.wbos.put(recordA2.guid, recordA2);
    repoA.wbos.put(recordA3.guid, recordA3);

    final Synchronizer s = new Synchronizer();
    s.repositoryA = repoA;
    s.repositoryB = repoB;

    Runnable r = new Runnable() {
      @Override
      public void run() {
        s.synchronize(getApplicationContext(), new SynchronizerDelegate() {

          @Override
          public void onSynchronized(Synchronizer synchronizer) {
            long countA = repoA.counter.get();
            long countB = repoB.counter.get();
            Logger.debug(getName(), "Counts: " + countA + ", " + countB);
            assertEq(2L, countA);
            assertEq(3L, countB);

            // Testing for store timestamp 'hack'.
            // We fetched from A first, and so its bundle timestamp will be the last
            // stored time. We fetched from B second, so its bundle timestamp will be
            // the last fetched time.
            final long timestampA = synchronizer.bundleA.getTimestamp();
            final long timestampB = synchronizer.bundleB.getTimestamp();
            Logger.debug(getName(), "Repo A timestamp: " + timestampA);
            Logger.debug(getName(), "Repo B timestamp: " + timestampB);
            Logger.debug(getName(), "Repo A fetch done: " + repoA.stats.fetchCompleted);
            Logger.debug(getName(), "Repo A store done: " + repoA.stats.storeCompleted);
            Logger.debug(getName(), "Repo B fetch done: " + repoB.stats.fetchCompleted);
            Logger.debug(getName(), "Repo B store done: " + repoB.stats.storeCompleted);

            assertTrue(timestampB <= timestampA);
            assertTrue(repoA.stats.fetchCompleted <= timestampA);
            assertTrue(repoA.stats.storeCompleted >= repoA.stats.fetchCompleted);
            assertEquals(repoA.stats.storeCompleted, timestampA);
            assertEquals(repoB.stats.fetchCompleted, timestampB);
            performNotify();
          }

          @Override
          public void onSynchronizeFailed(Synchronizer synchronizer,
                                          Exception lastException, String reason) {
            Logger.debug(getName(), "Failed.");
            performNotify(new AssertionFailedError("Should not fail."));
          }
        });
      }
    };

    performWait(onThreadRunnable(r));
  }
}
