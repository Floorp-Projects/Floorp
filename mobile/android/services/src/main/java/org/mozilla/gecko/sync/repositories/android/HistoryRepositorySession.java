/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.repositories.android;

import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.sync.repositories.InactiveSessionException;
import org.mozilla.gecko.sync.repositories.InvalidSessionTransitionException;
import org.mozilla.gecko.sync.repositories.NoStoreDelegateException;
import org.mozilla.gecko.sync.repositories.ProfileDatabaseException;
import org.mozilla.gecko.sync.repositories.Repository;
import org.mozilla.gecko.sync.repositories.StoreTrackingRepositorySession;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionBeginDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionFetchRecordsDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionFinishDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionWipeDelegate;
import org.mozilla.gecko.sync.repositories.domain.Record;

import android.content.Context;

public class HistoryRepositorySession extends StoreTrackingRepositorySession {
  public static final String LOG_TAG = "ABHistoryRepoSess";

  private final HistoryDataAccessor dbHelper;
  private final HistorySessionHelper sessionHelper;
  private int storeCount = 0;

  public HistoryRepositorySession(Repository repository, Context context) {
    super(repository);
    dbHelper = new HistoryDataAccessor(context);
    sessionHelper = new HistorySessionHelper(this, dbHelper);
  }

  @Override
  public void begin(RepositorySessionBeginDelegate delegate) throws InvalidSessionTransitionException {
    // HACK: Fennec creates history records without a GUID. Mercilessly drop
    // them on the floor. See Bug 739514.
    try {
      dbHelper.delete(BrowserContract.History.GUID + " IS NULL", null);
    } catch (Exception e) {
      // Ignore.
    }
    RepositorySessionBeginDelegate deferredDelegate = delegate.deferredBeginDelegate(delegateQueue);
    super.sharedBegin();

    try {
      // We do this check here even though it results in one extra call to the DB
      // because if we didn't, we have to do a check on every other call since there
      // is no way of knowing which call would be hit first.
      sessionHelper.checkDatabase();
    } catch (ProfileDatabaseException e) {
      Logger.error(LOG_TAG, "ProfileDatabaseException from begin. Fennec must be launched once until this error is fixed");
      deferredDelegate.onBeginFailed(e);
      return;
    } catch (Exception e) {
      deferredDelegate.onBeginFailed(e);
      return;
    }
    storeTracker = createStoreTracker();
    deferredDelegate.onBeginSucceeded(this);
  }

  @Override
  public void fetchModified(RepositorySessionFetchRecordsDelegate delegate) {
    this.fetchSince(getLastSyncTimestamp(), delegate);
  }

  @Override
  public void fetch(String[] guids, RepositorySessionFetchRecordsDelegate delegate) throws InactiveSessionException {
    executeDelegateCommand(sessionHelper.getFetchRunnable(guids, now(), null, delegate));
  }

  @Override
  public void fetchAll(RepositorySessionFetchRecordsDelegate delegate) {
    this.fetchSince(-1, delegate);
  }

  private void fetchSince(long timestamp, RepositorySessionFetchRecordsDelegate delegate) {
    if (this.storeTracker == null) {
      throw new IllegalStateException("Store tracker not yet initialized!");
    }

    Logger.debug(LOG_TAG, "Running fetchSince(" + timestamp + ").");
    delegateQueue.execute(
            sessionHelper.getFetchSinceRunnable(
                    timestamp, now(), this.storeTracker.getFilter(), delegate
            )
    );
  }

  @Override
  public void store(Record record) throws NoStoreDelegateException {
    if (storeDelegate == null) {
      throw new NoStoreDelegateException();
    }
    if (record == null) {
      Logger.error(LOG_TAG, "Record sent to store was null");
      throw new IllegalArgumentException("Null record passed to AndroidBrowserRepositorySession.store().");
    }

    storeCount += 1;
    Logger.debug(LOG_TAG, "Storing record with GUID " + record.guid + " (stored " + storeCount + " records this session).");

    // Store Runnables *must* complete synchronously. It's OK, they
    // run on a background thread.
    storeWorkQueue.execute(sessionHelper.getStoreRunnable(record, storeDelegate));
  }

  @Override
  public void wipe(RepositorySessionWipeDelegate delegate) {
    Runnable command = sessionHelper.getWipeRunnable(delegate);
    storeWorkQueue.execute(command);
  }

  /**
   * We need to flush our internal buffer of records in case of any interruptions of record flow
   * from our "source". Downloader might be maintaining a "high-water-mark" based on the records
   * it tried to store, so it's pertinent that all of the records that were queued for storage
   * are eventually persisted.
   */
  @Override
  public void storeIncomplete() {
    storeWorkQueue.execute(sessionHelper.getStoreIncompleteRunnable(storeDelegate));
  }

  @Override
  public void storeDone() {
    storeWorkQueue.execute(sessionHelper.getStoreDoneRunnable(storeDelegate));
    // Work queue is single-threaded, and so this should be well-ordered - onStoreComplete will run
    // after the above runnable is finished.
    storeWorkQueue.execute(new Runnable() {
      @Override
      public void run() {
        setLastStoreTimestamp(now());
        storeDelegate.onStoreCompleted();
      }
    });
  }

  @Override
  public void finish(RepositorySessionFinishDelegate delegate) throws InactiveSessionException {
    sessionHelper.finish();
    super.finish(delegate);
  }
}
