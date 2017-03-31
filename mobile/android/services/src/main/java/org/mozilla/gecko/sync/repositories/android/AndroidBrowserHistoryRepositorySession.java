/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.repositories.android;

import java.util.ArrayList;

import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.sync.repositories.InvalidSessionTransitionException;
import org.mozilla.gecko.sync.repositories.NoGuidForIdException;
import org.mozilla.gecko.sync.repositories.NullCursorException;
import org.mozilla.gecko.sync.repositories.ParentNotFoundException;
import org.mozilla.gecko.sync.repositories.Repository;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionBeginDelegate;
import org.mozilla.gecko.sync.repositories.domain.HistoryRecord;
import org.mozilla.gecko.sync.repositories.domain.Record;

import android.content.ContentProviderClient;
import android.content.Context;
import android.database.Cursor;
import android.os.RemoteException;

public class AndroidBrowserHistoryRepositorySession extends AndroidBrowserRepositorySession {
  public static final String LOG_TAG = "ABHistoryRepoSess";

  /**
   * The number of records to queue for insertion before writing to databases.
   */
  public static final int INSERT_RECORD_THRESHOLD = 5000;
  public static final int RECENT_VISITS_LIMIT = 20;

  public AndroidBrowserHistoryRepositorySession(Repository repository, Context context) {
    super(repository);
    dbHelper = new AndroidBrowserHistoryDataAccessor(context);
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
    super.begin(delegate);
  }

  @Override
  protected Record retrieveDuringStore(Cursor cur) {
    return RepoUtils.historyFromMirrorCursor(cur);
  }

  @Override
  protected Record retrieveDuringFetch(Cursor cur) {
    return RepoUtils.historyFromMirrorCursor(cur);
  }

  @Override
  protected String buildRecordString(Record record) {
    HistoryRecord hist = (HistoryRecord) record;
    return hist.histURI;
  }

  @Override
  public boolean shouldIgnore(Record record) {
    if (super.shouldIgnore(record)) {
      return true;
    }
    if (!(record instanceof HistoryRecord)) {
      return true;
    }
    HistoryRecord r = (HistoryRecord) record;
    return !RepoUtils.isValidHistoryURI(r.histURI);
  }

  @Override
  protected Record transformRecord(Record record) throws NullCursorException {
    return addVisitsToRecord(record);
  }

  private Record addVisitsToRecord(Record record) throws NullCursorException {
    Logger.debug(LOG_TAG, "Adding visits for GUID " + record.guid);

    // Sync is an object store, so what we attach here will replace what's already present on the Sync servers.
    // We upload just a recent subset of visits for each history record for space and bandwidth reasons.
    // We chose 20 to be conservative.  See Bug 1164660 for details.
    ContentProviderClient visitsClient = dbHelper.context.getContentResolver().acquireContentProviderClient(BrowserContractHelpers.VISITS_CONTENT_URI);
    if (visitsClient == null) {
      throw new IllegalStateException("Could not obtain a ContentProviderClient for Visits URI");
    }

    try {
      ((HistoryRecord) record).visits = VisitsHelper.getRecentHistoryVisitsForGUID(
              visitsClient, record.guid, RECENT_VISITS_LIMIT);
    } catch (RemoteException e) {
      throw new IllegalStateException("Error while obtaining visits for a record", e);
    } finally {
      visitsClient.release();
    }

    return record;
  }

  @Override
  protected Record prepareRecord(Record record) {
    return record;
  }

  protected final Object recordsBufferMonitor = new Object();
  protected ArrayList<HistoryRecord> recordsBuffer = new ArrayList<HistoryRecord>();

  /**
   * Queue record for insertion, possibly flushing the queue.
   * <p>
   * Must be called on <code>storeWorkQueue</code> thread! But this is only
   * called from <code>store</code>, which is called on the queue thread.
   *
   * @param record
   *          A <code>Record</code> with a GUID that is not present locally.
   */
  @Override
  protected void insert(Record record) throws NoGuidForIdException, NullCursorException, ParentNotFoundException {
    enqueueNewRecord((HistoryRecord) prepareRecord(record));
  }

  /**
   * Batch incoming records until some reasonable threshold is hit or storeDone
   * is received.
   * <p>
   * Must be called on <code>storeWorkQueue</code> thread!
   *
   * @param record A <code>Record</code> with a GUID that is not present locally.
   * @throws NullCursorException
   */
  protected void enqueueNewRecord(HistoryRecord record) throws NullCursorException {
    synchronized (recordsBufferMonitor) {
      if (recordsBuffer.size() >= INSERT_RECORD_THRESHOLD) {
        flushNewRecords();
      }
      Logger.debug(LOG_TAG, "Enqueuing new record with GUID " + record.guid);
      recordsBuffer.add(record);
    }
  }

  /**
   * Flush queue of incoming records to database.
   * <p>
   * Must be called on <code>storeWorkQueue</code> thread!
   * <p>
   * Must be locked by recordsBufferMonitor!
   * @throws NullCursorException
   */
  protected void flushNewRecords() throws NullCursorException {
    if (recordsBuffer.size() < 1) {
      Logger.debug(LOG_TAG, "No records to flush, returning.");
      return;
    }

    final ArrayList<HistoryRecord> outgoing = recordsBuffer;
    recordsBuffer = new ArrayList<HistoryRecord>();
    Logger.debug(LOG_TAG, "Flushing " + outgoing.size() + " records to database.");
    boolean transactionSuccess = ((AndroidBrowserHistoryDataAccessor) dbHelper).bulkInsert(outgoing);
    if (!transactionSuccess) {
      for (HistoryRecord failed : outgoing) {
        storeDelegate.onRecordStoreFailed(new RuntimeException("Failed to insert history item with guid " + failed.guid + "."), failed.guid);
      }
      return;
    }

    // All good, everybody succeeded.
    for (HistoryRecord succeeded : outgoing) {
      try {
        // Does not use androidID -- just GUID -> String map.
        updateBookkeeping(succeeded);
      } catch (NoGuidForIdException | ParentNotFoundException e) {
        // Should not happen.
        throw new NullCursorException(e);
      } catch (NullCursorException e) {
        throw e;
      }
      trackRecord(succeeded);
      storeDelegate.onRecordStoreSucceeded(succeeded.guid); // At this point, we are really inserted.
    }
  }

  /**
   * We need to flush our internal buffer of records in case of any interruptions of record flow
   * from our "source". Downloader might be maintaining a "high-water-mark" based on the records
   * it tried to store, so it's pertinent that all of the records that were queued for storage
   * are eventually persisted.
   */
  @Override
  public void storeIncomplete() {
    storeWorkQueue.execute(new Runnable() {
      @Override
      public void run() {
        synchronized (recordsBufferMonitor) {
          try {
            flushNewRecords();
          } catch (Exception e) {
            Logger.warn(LOG_TAG, "Error flushing records to database.", e);
          }
        }
      }
    });
  }

  @Override
  public void storeDone() {
    storeWorkQueue.execute(new Runnable() {
      @Override
      public void run() {
        synchronized (recordsBufferMonitor) {
          try {
            flushNewRecords();
          } catch (Exception e) {
            Logger.warn(LOG_TAG, "Error flushing records to database.", e);
          }
        }
        storeDelegate.deferredStoreDelegate(storeWorkQueue).onStoreCompleted(now());
      }
    });
  }
}
