/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.android.sync.test;

import android.content.Context;
import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.background.testhelpers.WBORepository;
import org.mozilla.gecko.sync.repositories.FetchFailedException;
import org.mozilla.gecko.sync.repositories.InactiveSessionException;
import org.mozilla.gecko.sync.repositories.InvalidSessionTransitionException;
import org.mozilla.gecko.sync.repositories.NoStoreDelegateException;
import org.mozilla.gecko.sync.repositories.StoreFailedException;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionBeginDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionCreationDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionFetchRecordsDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionFinishDelegate;
import org.mozilla.gecko.sync.repositories.domain.Record;

import java.util.ArrayList;
import java.util.concurrent.ExecutorService;

public class SynchronizerHelpers {
  public static final String FAIL_SENTINEL = "Fail";

  /**
   * Store one at a time, failing if the guid contains FAIL_SENTINEL.
   */
  public static class FailFetchWBORepository extends WBORepository {
    @Override
    public void createSession(RepositorySessionCreationDelegate delegate,
                              Context context) {
      delegate.deferredCreationDelegate().onSessionCreated(new WBORepositorySession(this) {
        @Override
        public void fetchSince(long timestamp,
                               final RepositorySessionFetchRecordsDelegate delegate) {
          super.fetchSince(timestamp, new RepositorySessionFetchRecordsDelegate() {
            @Override
            public void onFetchedRecord(Record record) {
              if (record.guid.contains(FAIL_SENTINEL)) {
                delegate.onFetchFailed(new FetchFailedException(), record);
              } else {
                delegate.onFetchedRecord(record);
              }
            }

            @Override
            public void onFetchFailed(Exception ex, Record record) {
              delegate.onFetchFailed(ex, record);
            }

            @Override
            public void onFetchCompleted(long fetchEnd) {
              delegate.onFetchCompleted(fetchEnd);
            }

            @Override
            public RepositorySessionFetchRecordsDelegate deferredFetchDelegate(ExecutorService executor) {
              return this;
            }
          });
        }
      });
    }
  }

  /**
   * Store one at a time, failing if the guid contains FAIL_SENTINEL.
   */
  public static class SerialFailStoreWBORepository extends WBORepository {
    @Override
    public void createSession(RepositorySessionCreationDelegate delegate,
                              Context context) {
      delegate.deferredCreationDelegate().onSessionCreated(new WBORepositorySession(this) {
        @Override
        public void store(final Record record) throws NoStoreDelegateException {
          if (storeDelegate == null) {
            throw new NoStoreDelegateException();
          }
          if (record.guid.contains(FAIL_SENTINEL)) {
            storeDelegate.onRecordStoreFailed(new StoreFailedException(), record.guid);
          } else {
            super.store(record);
          }
        }
      });
    }
  }

  /**
   * Store in batches, failing if any of the batch guids contains "Fail".
   * <p>
   * This will drop the final batch.
   */
  public static class BatchFailStoreWBORepository extends WBORepository {
    public final int batchSize;
    public ArrayList<Record> batch = new ArrayList<Record>();
    public boolean batchShouldFail = false;

    public class BatchFailStoreWBORepositorySession extends WBORepositorySession {
      public BatchFailStoreWBORepositorySession(WBORepository repository) {
        super(repository);
      }

      public void superStore(final Record record) throws NoStoreDelegateException {
        super.store(record);
      }

      @Override
      public void store(final Record record) throws NoStoreDelegateException {
        if (storeDelegate == null) {
          throw new NoStoreDelegateException();
        }
        synchronized (batch) {
          batch.add(record);
          if (record.guid.contains("Fail")) {
            batchShouldFail = true;
          }

          if (batch.size() >= batchSize) {
            flush();
          }
        }
      }

      public void flush() {
        final ArrayList<Record> thisBatch = new ArrayList<Record>(batch);
        final boolean thisBatchShouldFail = batchShouldFail;
        batchShouldFail = false;
        batch.clear();
        storeWorkQueue.execute(new Runnable() {
          @Override
          public void run() {
            Logger.trace("XXX", "Notifying about batch.  Failure? " + thisBatchShouldFail);
            for (Record batchRecord : thisBatch) {
              if (thisBatchShouldFail) {
                storeDelegate.onRecordStoreFailed(new StoreFailedException(), batchRecord.guid);
              } else {
                try {
                  superStore(batchRecord);
                } catch (NoStoreDelegateException e) {
                  storeDelegate.onRecordStoreFailed(e, batchRecord.guid);
                }
              }
            }
          }
        });
      }

      @Override
      public void storeDone() {
        synchronized (batch) {
          flush();
          // Do this in a Runnable so that the timestamp is grabbed after any upload.
          final Runnable r = new Runnable() {
            @Override
            public void run() {
              synchronized (batch) {
                Logger.trace("XXX", "Calling storeDone.");
                storeDone(now());
              }
            }
          };
          storeWorkQueue.execute(r);
        }
      }
    }
    public BatchFailStoreWBORepository(int batchSize) {
      super();
      this.batchSize = batchSize;
    }

    @Override
    public void createSession(RepositorySessionCreationDelegate delegate,
                              Context context) {
      delegate.deferredCreationDelegate().onSessionCreated(new BatchFailStoreWBORepositorySession(this));
    }
  }

  public static class TrackingWBORepository extends WBORepository {
    @Override
    public synchronized boolean shouldTrack() {
      return true;
    }
  }

  public static class BeginFailedException extends Exception {
    private static final long serialVersionUID = -2349459755976915096L;
  }

  public static class FinishFailedException extends Exception {
    private static final long serialVersionUID = -4644528423867070934L;
  }

  public static class BeginErrorWBORepository extends TrackingWBORepository {
    @Override
    public void createSession(RepositorySessionCreationDelegate delegate,
                              Context context) {
      delegate.deferredCreationDelegate().onSessionCreated(new BeginErrorWBORepositorySession(this));
    }

    public class BeginErrorWBORepositorySession extends WBORepositorySession {
      public BeginErrorWBORepositorySession(WBORepository repository) {
        super(repository);
      }

      @Override
      public void begin(RepositorySessionBeginDelegate delegate) throws InvalidSessionTransitionException {
        delegate.onBeginFailed(new BeginFailedException());
      }
    }
  }

  public static class FinishErrorWBORepository extends TrackingWBORepository {
    @Override
    public void createSession(RepositorySessionCreationDelegate delegate,
                              Context context) {
      delegate.deferredCreationDelegate().onSessionCreated(new FinishErrorWBORepositorySession(this));
    }

    public class FinishErrorWBORepositorySession extends WBORepositorySession {
      public FinishErrorWBORepositorySession(WBORepository repository) {
        super(repository);
      }

      @Override
      public void finish(final RepositorySessionFinishDelegate delegate) throws InactiveSessionException {
        delegate.onFinishFailed(new FinishFailedException());
      }
    }
  }

  public static class DataAvailableWBORepository extends TrackingWBORepository {
    public boolean dataAvailable = true;

    public DataAvailableWBORepository(boolean dataAvailable) {
      this.dataAvailable = dataAvailable;
    }

    @Override
    public void createSession(RepositorySessionCreationDelegate delegate,
                              Context context) {
      delegate.deferredCreationDelegate().onSessionCreated(new DataAvailableWBORepositorySession(this));
    }

    public class DataAvailableWBORepositorySession extends WBORepositorySession {
      public DataAvailableWBORepositorySession(WBORepository repository) {
        super(repository);
      }

      @Override
      public boolean dataAvailable() {
        return dataAvailable;
      }
    }
  }

  public static class ShouldSkipWBORepository extends TrackingWBORepository {
    public boolean shouldSkip = true;

    public ShouldSkipWBORepository(boolean shouldSkip) {
      this.shouldSkip = shouldSkip;
    }

    @Override
    public void createSession(RepositorySessionCreationDelegate delegate,
                              Context context) {
      delegate.deferredCreationDelegate().onSessionCreated(new ShouldSkipWBORepositorySession(this));
    }

    public class ShouldSkipWBORepositorySession extends WBORepositorySession {
      public ShouldSkipWBORepositorySession(WBORepository repository) {
        super(repository);
      }

      @Override
      public boolean shouldSkip() {
        return shouldSkip;
      }
    }
  }
}
