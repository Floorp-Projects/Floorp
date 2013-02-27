/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.repositories.android;

import java.util.ArrayList;

import org.json.simple.JSONArray;
import org.json.simple.JSONObject;
import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.sync.repositories.InactiveSessionException;
import org.mozilla.gecko.sync.repositories.InvalidSessionTransitionException;
import org.mozilla.gecko.sync.repositories.NoGuidForIdException;
import org.mozilla.gecko.sync.repositories.NullCursorException;
import org.mozilla.gecko.sync.repositories.ParentNotFoundException;
import org.mozilla.gecko.sync.repositories.Repository;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionBeginDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionFinishDelegate;
import org.mozilla.gecko.sync.repositories.domain.HistoryRecord;
import org.mozilla.gecko.sync.repositories.domain.Record;

import android.content.Context;
import android.database.Cursor;

public class AndroidBrowserHistoryRepositorySession extends AndroidBrowserRepositorySession {
  public static final String LOG_TAG = "ABHistoryRepoSess";

  public static final String KEY_DATE = "date";
  public static final String KEY_TYPE = "type";
  public static final long DEFAULT_VISIT_TYPE = 1;

  /**
   * The number of records to queue for insertion before writing to databases.
   */
  public static int INSERT_RECORD_THRESHOLD = 50;

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

  @SuppressWarnings("unchecked")
  private void addVisit(JSONArray visits, long date, long visitType) {
    JSONObject visit = new JSONObject();
    visit.put(KEY_DATE, date);               // Microseconds since epoch.
    visit.put(KEY_TYPE, visitType);
    visits.add(visit);
  }

  private void addVisit(JSONArray visits, long date) {
    addVisit(visits, date, DEFAULT_VISIT_TYPE);
  }

  private AndroidBrowserHistoryDataExtender getDataExtender() {
    return ((AndroidBrowserHistoryDataAccessor) dbHelper).getHistoryDataExtender();
  }

  private Record addVisitsToRecord(Record record) throws NullCursorException {
    Logger.debug(LOG_TAG, "Adding visits for GUID " + record.guid);
    HistoryRecord hist = (HistoryRecord) record;
    JSONArray visitsArray = getDataExtender().visitsForGUID(hist.guid);
    long missingRecords = hist.fennecVisitCount - visitsArray.size();

    // Note that Fennec visit times are milliseconds, and we are working
    // in microseconds. This is the point at which we translate.

    // Add (missingRecords - 1) fake visits...
    if (missingRecords > 0) {
      long fakes = missingRecords - 1;
      for (int j = 0; j < fakes; j++) {
        // Set fake visit timestamp to be just previous to
        // the real one we are about to add.
        // TODO: make these equidistant?
        long fakeDate = (hist.fennecDateVisited - (1 + j)) * 1000;
        addVisit(visitsArray, fakeDate);
      }

      // ... and the 1 actual record we have.
      // We still have to fake the visit type: Fennec doesn't track that.
      addVisit(visitsArray, hist.fennecDateVisited * 1000);
    }

    hist.visits = visitsArray;
    return hist;
  }

  @Override
  protected Record prepareRecord(Record record) {
    return record;
  }

  @Override
  public void abort() {
    if (dbHelper != null) {
      ((AndroidBrowserHistoryDataAccessor) dbHelper).closeExtender();
      dbHelper = null;
    }
    super.abort();
  }

  @Override
  public void finish(final RepositorySessionFinishDelegate delegate) throws InactiveSessionException {
    if (dbHelper != null) {
      ((AndroidBrowserHistoryDataAccessor) dbHelper).closeExtender();
      dbHelper = null;
    }
    super.finish(delegate);
  }

  protected Object recordsBufferMonitor = new Object();
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
    // TODO: move bulkInsert to AndroidBrowserDataAccessor?
    int inserted = ((AndroidBrowserHistoryDataAccessor) dbHelper).bulkInsert(outgoing);
    if (inserted != outgoing.size()) {
      // Something failed; most pessimistic action is to declare that all insertions failed.
      // TODO: perform the bulkInsert in a transaction and rollback unless all insertions succeed?
      for (HistoryRecord failed : outgoing) {
        delegate.onRecordStoreFailed(new RuntimeException("Failed to insert history item with guid " + failed.guid + "."), failed.guid);
      }
      return;
    }

    // All good, everybody succeeded.
    for (HistoryRecord succeeded : outgoing) {
      try {
        // Does not use androidID -- just GUID -> String map.
        updateBookkeeping(succeeded);
      } catch (NoGuidForIdException e) {
        // Should not happen.
        throw new NullCursorException(e);
      } catch (ParentNotFoundException e) {
        // Should not happen.
        throw new NullCursorException(e);
      } catch (NullCursorException e) {
        throw e;
      }
      trackRecord(succeeded);
      delegate.onRecordStoreSucceeded(succeeded.guid); // At this point, we are really inserted.
    }
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
        storeDone(System.currentTimeMillis());
      }
    });
  }
}
