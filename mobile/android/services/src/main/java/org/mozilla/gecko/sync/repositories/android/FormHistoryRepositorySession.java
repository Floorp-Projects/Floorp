/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.repositories.android;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.Callable;

import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.db.BrowserContract.DeletedFormHistory;
import org.mozilla.gecko.db.BrowserContract.FormHistory;
import org.mozilla.gecko.sync.repositories.InactiveSessionException;
import org.mozilla.gecko.sync.repositories.NoContentProviderException;
import org.mozilla.gecko.sync.repositories.NoStoreDelegateException;
import org.mozilla.gecko.sync.repositories.NullCursorException;
import org.mozilla.gecko.sync.repositories.RecordFilter;
import org.mozilla.gecko.sync.repositories.Repository;
import org.mozilla.gecko.sync.repositories.StoreTrackingRepositorySession;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionCreationDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionFetchRecordsDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionFinishDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionGuidsSinceDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionWipeDelegate;
import org.mozilla.gecko.sync.repositories.domain.FormHistoryRecord;
import org.mozilla.gecko.sync.repositories.domain.Record;

import android.content.ContentProviderClient;
import android.content.ContentValues;
import android.content.Context;
import android.database.Cursor;
import android.net.Uri;
import android.os.RemoteException;

public class FormHistoryRepositorySession extends
    StoreTrackingRepositorySession {
  public static final String LOG_TAG = "FormHistoryRepoSess";

  /**
   * Number of records to insert in one batch.
   */
  public static final int INSERT_ITEM_THRESHOLD = 200;

  private static final Uri FORM_HISTORY_CONTENT_URI = BrowserContractHelpers.FORM_HISTORY_CONTENT_URI;
  private static final Uri DELETED_FORM_HISTORY_CONTENT_URI = BrowserContractHelpers.DELETED_FORM_HISTORY_CONTENT_URI;

  public static class FormHistoryRepository extends Repository {

    @Override
    public void createSession(RepositorySessionCreationDelegate delegate,
                              Context context) {
      try {
        final FormHistoryRepositorySession session = new FormHistoryRepositorySession(this, context);
        delegate.onSessionCreated(session);
      } catch (Exception e) {
        delegate.onSessionCreateFailed(e);
      }
    }
  }

  protected final ContentProviderClient formsProvider;
  protected final RepoUtils.QueryHelper regularHelper;
  protected final RepoUtils.QueryHelper deletedHelper;

  /**
   * Acquire the content provider client.
   * <p>
   * The caller is responsible for releasing the client.
   *
   * @param context The application context.
   * @return The <code>ContentProviderClient</code>.
   * @throws NoContentProviderException
   */
  public static ContentProviderClient acquireContentProvider(final Context context)
      throws NoContentProviderException {
    Uri uri = BrowserContract.FORM_HISTORY_AUTHORITY_URI;
    ContentProviderClient client = context.getContentResolver().acquireContentProviderClient(uri);
    if (client == null) {
      throw new NoContentProviderException(uri);
    }
    return client;
  }

  protected void releaseProviders() {
    try {
      if (formsProvider != null) {
        formsProvider.release();
      }
    } catch (Exception e) {
    }
  }

  // Only used for testing.
  public ContentProviderClient getFormsProvider() {
    return formsProvider;
  }

  public FormHistoryRepositorySession(Repository repository, Context context)
      throws NoContentProviderException {
    super(repository);
    formsProvider = acquireContentProvider(context);
    regularHelper = new RepoUtils.QueryHelper(context, BrowserContractHelpers.FORM_HISTORY_CONTENT_URI, LOG_TAG);
    deletedHelper = new RepoUtils.QueryHelper(context, BrowserContractHelpers.DELETED_FORM_HISTORY_CONTENT_URI, LOG_TAG);
  }

  @Override
  public void abort() {
    releaseProviders();
    super.abort();
  }

  @Override
  public void finish(final RepositorySessionFinishDelegate delegate)
      throws InactiveSessionException {
    releaseProviders();
    super.finish(delegate);
  }

  protected static final String[] GUID_COLUMNS = new String[] { FormHistory.GUID };

  @Override
  public void guidsSince(final long timestamp, final RepositorySessionGuidsSinceDelegate delegate) {
    Runnable command = new Runnable() {
      @Override
      public void run() {
        if (!isActive()) {
          delegate.onGuidsSinceFailed(new InactiveSessionException());
          return;
        }

        ArrayList<String> guids = new ArrayList<String>();

        final long sharedEnd = now();
        Cursor cur = null;
        try {
          cur = regularHelper.safeQuery(formsProvider, "", GUID_COLUMNS, regularBetween(timestamp, sharedEnd), null, null);
          cur.moveToFirst();
          while (!cur.isAfterLast()) {
            guids.add(cur.getString(0));
            cur.moveToNext();
          }
        } catch (RemoteException | NullCursorException e) {
          delegate.onGuidsSinceFailed(e);
          return;
        } finally {
          if (cur != null) {
            cur.close();
          }
        }

        try {
          cur = deletedHelper.safeQuery(formsProvider, "", GUID_COLUMNS, deletedBetween(timestamp, sharedEnd), null, null);
          cur.moveToFirst();
          while (!cur.isAfterLast()) {
            guids.add(cur.getString(0));
            cur.moveToNext();
          }
        } catch (RemoteException | NullCursorException e) {
          delegate.onGuidsSinceFailed(e);
          return;
        } finally {
          if (cur != null) {
            cur.close();
          }
        }

        String guidsArray[] = guids.toArray(new String[guids.size()]);
        delegate.onGuidsSinceSucceeded(guidsArray);
      }
    };
    delegateQueue.execute(command);
  }

  protected static FormHistoryRecord retrieveDuringFetch(final Cursor cursor) {
    // A simple and efficient way to distinguish two tables.
    if (cursor.getColumnCount() == BrowserContractHelpers.FormHistoryColumns.length) {
      return formHistoryRecordFromCursor(cursor);
    } else {
      return deletedFormHistoryRecordFromCursor(cursor);
    }
  }

  protected static FormHistoryRecord formHistoryRecordFromCursor(final Cursor cursor) {
    String guid = RepoUtils.getStringFromCursor(cursor, FormHistory.GUID);
    String collection = "forms";
    FormHistoryRecord record = new FormHistoryRecord(guid, collection, 0, false);

    record.fieldName = RepoUtils.getStringFromCursor(cursor, FormHistory.FIELD_NAME);
    record.fieldValue = RepoUtils.getStringFromCursor(cursor, FormHistory.VALUE);
    record.androidID = RepoUtils.getLongFromCursor(cursor, FormHistory.ID);
    record.lastModified = RepoUtils.getLongFromCursor(cursor, FormHistory.FIRST_USED) / 1000; // Convert microseconds to milliseconds.
    record.deleted = false;

    record.log(LOG_TAG);
    return record;
  }

  protected static FormHistoryRecord deletedFormHistoryRecordFromCursor(final Cursor cursor) {
    String guid = RepoUtils.getStringFromCursor(cursor, DeletedFormHistory.GUID);
    String collection = "forms";
    FormHistoryRecord record = new FormHistoryRecord(guid, collection, 0, false);

    record.guid = RepoUtils.getStringFromCursor(cursor, DeletedFormHistory.GUID);
    record.androidID = RepoUtils.getLongFromCursor(cursor, DeletedFormHistory.ID);
    record.lastModified = RepoUtils.getLongFromCursor(cursor, DeletedFormHistory.TIME_DELETED);
    record.deleted = true;

    record.log(LOG_TAG);
    return record;
  }

  protected static void fetchFromCursor(final Cursor cursor, final RecordFilter filter, final RepositorySessionFetchRecordsDelegate delegate)
      throws NullCursorException {
    Logger.debug(LOG_TAG, "Fetch from cursor");
    if (cursor == null) {
      throw new NullCursorException(null);
    }
    try {
      if (!cursor.moveToFirst()) {
        return;
      }
      while (!cursor.isAfterLast()) {
        Record r = retrieveDuringFetch(cursor);
        if (r != null) {
          if (filter == null || !filter.excludeRecord(r)) {
            Logger.trace(LOG_TAG, "Processing record " + r.guid);
            delegate.onFetchedRecord(r);
          } else {
            Logger.debug(LOG_TAG, "Skipping filtered record " + r.guid);
          }
        }
        cursor.moveToNext();
      }
    } finally {
      Logger.trace(LOG_TAG, "Closing cursor after fetch.");
      cursor.close();
    }
  }

  protected void fetchHelper(final RepositorySessionFetchRecordsDelegate delegate, final long end, final List<Callable<Cursor>> cursorCallables) {
    if (this.storeTracker == null) {
      throw new IllegalStateException("Store tracker not yet initialized!");
    }

    final RecordFilter filter = this.storeTracker.getFilter();

    Runnable command = new Runnable() {
      @Override
      public void run() {
        if (!isActive()) {
          delegate.onFetchFailed(new InactiveSessionException());
          return;
        }

        for (Callable<Cursor> cursorCallable : cursorCallables) {
          Cursor cursor = null;
          try {
            cursor = cursorCallable.call();
            fetchFromCursor(cursor, filter, delegate); // Closes cursor.
          } catch (Exception e) {
            Logger.warn(LOG_TAG, "Exception during fetchHelper", e);
            delegate.onFetchFailed(e);
            return;
          }
        }

        delegate.onFetchCompleted(end);
      }
    };

    delegateQueue.execute(command);
  }

  protected static String regularBetween(long start, long end) {
    return FormHistory.FIRST_USED + " >= " + Long.toString(1000 * start) + " AND " +
           FormHistory.FIRST_USED + " <= " + Long.toString(1000 * end); // Microseconds.
  }

  protected static String deletedBetween(long start, long end) {
    return DeletedFormHistory.TIME_DELETED + " >= " + Long.toString(start) + " AND " +
           DeletedFormHistory.TIME_DELETED + " <= " + Long.toString(end); // Milliseconds.
  }

  @Override
  public void fetchSince(final long timestamp, final RepositorySessionFetchRecordsDelegate delegate) {
    Logger.trace(LOG_TAG, "Running fetchSince(" + timestamp + ").");

    /*
     * We need to be careful about the timestamp we complete the fetch with. If
     * the first cursor Callable takes a year, then the second could return
     * records long after the first was kicked off. To protect against this, we
     * set an end point and bound our search.
     */
    final long sharedEnd = now();

    Callable<Cursor> regularCallable = new Callable<Cursor>() {
      @Override
      public Cursor call() throws Exception {
        return regularHelper.safeQuery(formsProvider, ".fetchSince(regular)", null, regularBetween(timestamp, sharedEnd), null, null);
      }
    };

    Callable<Cursor> deletedCallable = new Callable<Cursor>() {
      @Override
      public Cursor call() throws Exception {
        return deletedHelper.safeQuery(formsProvider, ".fetchSince(deleted)", null, deletedBetween(timestamp, sharedEnd), null, null);
      }
    };

    @SuppressWarnings("unchecked")
    List<Callable<Cursor>> callableCursors = Arrays.asList(regularCallable, deletedCallable);

    fetchHelper(delegate, sharedEnd, callableCursors);
  }

  @Override
  public void fetchAll(RepositorySessionFetchRecordsDelegate delegate) {
    Logger.trace(LOG_TAG, "Running fetchAll.");
    fetchSince(0, delegate);
  }

  @Override
  public void fetch(final String[] guids, final RepositorySessionFetchRecordsDelegate delegate) {
    Logger.trace(LOG_TAG, "Running fetch.");

    final long sharedEnd = now();
    final String where = RepoUtils.computeSQLInClause(guids.length, FormHistory.GUID);

    Callable<Cursor> regularCallable = new Callable<Cursor>() {
      @Override
      public Cursor call() throws Exception {
        String regularWhere = where + " AND " + FormHistory.FIRST_USED + " <= " + Long.toString(1000 * sharedEnd); // Microseconds.
        return regularHelper.safeQuery(formsProvider, ".fetch(regular)", null, regularWhere, guids, null);
      }
    };

    Callable<Cursor> deletedCallable = new Callable<Cursor>() {
      @Override
      public Cursor call() throws Exception {
        String deletedWhere = where + " AND " + DeletedFormHistory.TIME_DELETED + " <= " + Long.toString(sharedEnd); // Milliseconds.
        return deletedHelper.safeQuery(formsProvider, ".fetch(deleted)", null, deletedWhere, guids, null);
      }
    };

    @SuppressWarnings("unchecked")
    List<Callable<Cursor>> callableCursors = Arrays.asList(regularCallable, deletedCallable);

    fetchHelper(delegate, sharedEnd, callableCursors);
  }

  protected static final String GUID_IS = FormHistory.GUID + " = ?";

  protected Record findExistingRecordByGuid(String guid)
      throws RemoteException, NullCursorException {
    Cursor cursor = null;
    try {
      cursor = regularHelper.safeQuery(formsProvider, ".findExistingRecordByGuid(regular)",
          null, GUID_IS, new String[] { guid }, null);
      if (cursor.moveToFirst()) {
        return formHistoryRecordFromCursor(cursor);
      }
    } finally {
      if (cursor != null) {
        cursor.close();
      }
    }

    try {
      cursor = deletedHelper.safeQuery(formsProvider, ".findExistingRecordByGuid(deleted)",
          null, GUID_IS, new String[] { guid }, null);
      if (cursor.moveToFirst()) {
        return deletedFormHistoryRecordFromCursor(cursor);
      }
    } finally {
      if (cursor != null) {
        cursor.close();
      }
    }

    return null;
  }

  protected Record findExistingRecordByPayload(Record rawRecord)
      throws RemoteException, NullCursorException {
    if (!rawRecord.deleted) {
      FormHistoryRecord record = (FormHistoryRecord) rawRecord;
      Cursor cursor = null;
      try {
        String where = FormHistory.FIELD_NAME + " = ? AND " + FormHistory.VALUE + " = ?";
        cursor = regularHelper.safeQuery(formsProvider, ".findExistingRecordByPayload",
            null, where, new String[] { record.fieldName, record.fieldValue }, null);
        if (cursor.moveToFirst()) {
          return formHistoryRecordFromCursor(cursor);
        }
      } finally {
        if (cursor != null) {
          cursor.close();
        }
      }
    }

    return null;
  }

  /**
   * Called when a record with locally known GUID has been reported deleted by
   * the server.
   * <p>
   * We purge the record's GUID from the regular and deleted tables.
   *
   * @param existingRecord
   *          The local <code>Record</code> to replace.
   * @throws RemoteException
   */
  protected void deleteExistingRecord(Record existingRecord) throws RemoteException {
    if (existingRecord.deleted) {
      formsProvider.delete(DELETED_FORM_HISTORY_CONTENT_URI, GUID_IS, new String[] { existingRecord.guid });
      return;
    }
    formsProvider.delete(FORM_HISTORY_CONTENT_URI, GUID_IS, new String[] { existingRecord.guid });
  }

  protected static ContentValues contentValuesForRegularRecord(Record rawRecord) {
    if (rawRecord.deleted) {
      throw new IllegalArgumentException("Deleted record passed to insertNewRegularRecord.");
    }

    FormHistoryRecord record = (FormHistoryRecord) rawRecord;
    ContentValues cv = new ContentValues();
    cv.put(FormHistory.GUID, record.guid);
    cv.put(FormHistory.FIELD_NAME, record.fieldName);
    cv.put(FormHistory.VALUE, record.fieldValue);
    cv.put(FormHistory.FIRST_USED, 1000 * record.lastModified); // Microseconds.
    return cv;
  }

  protected final Object recordsBufferMonitor = new Object();
  protected ArrayList<ContentValues> recordsBuffer = new ArrayList<ContentValues>();

  protected void enqueueRegularRecord(Record record) {
    synchronized (recordsBufferMonitor) {
      if (recordsBuffer.size() >= INSERT_ITEM_THRESHOLD) {
        // Insert the existing contents, then enqueue.
        try {
          flushInsertQueue();
        } catch (Exception e) {
          storeDelegate.onRecordStoreFailed(e, record.guid);
          return;
        }
      }
      // Store the ContentValues, rather than the record.
      recordsBuffer.add(contentValuesForRegularRecord(record));
    }
  }

  // Should always be called from storeWorkQueue.
  protected void flushInsertQueue() throws RemoteException {
    synchronized (recordsBufferMonitor) {
      if (recordsBuffer.size() > 0) {
        final ContentValues[] outgoing = recordsBuffer.toArray(new ContentValues[recordsBuffer.size()]);
        recordsBuffer = new ArrayList<ContentValues>();

        if (outgoing == null || outgoing.length == 0) {
          Logger.debug(LOG_TAG, "No form history items to insert; returning immediately.");
          return;
        }

        long before = System.currentTimeMillis();
        formsProvider.bulkInsert(FORM_HISTORY_CONTENT_URI, outgoing);
        long after = System.currentTimeMillis();
        Logger.debug(LOG_TAG, "Inserted " + outgoing.length + " form history items in (" + (after - before) + " milliseconds).");
      }
    }
  }

  @Override
  public void storeDone() {
    Runnable command = new Runnable() {
      @Override
      public void run() {
        Logger.debug(LOG_TAG, "Checking for residual form history items to insert.");
        try {
          synchronized (recordsBufferMonitor) {
            flushInsertQueue();
          }
          storeDone(now());
        } catch (Exception e) {
          // XXX TODO
          storeDelegate.onRecordStoreFailed(e, null);
        }
      }
    };
    storeWorkQueue.execute(command);
  }

  /**
   * Called when a regular record with locally unknown GUID has been fetched
   * from the server.
   * <p>
   * Since the record is regular, we insert it into the regular table.
   *
   * @param record The regular <code>Record</code> from the server.
   * @throws RemoteException
   */
  protected void insertNewRegularRecord(Record record)
      throws RemoteException {
    enqueueRegularRecord(record);
  }

  /**
   * Called when a regular record with has been fetched from the server and
   * should replace an existing record.
   * <p>
   * We delete the existing record entirely, and then insert the new record into
   * the regular table.
   *
   * @param toStore
   *          The regular <code>Record</code> from the server.
   * @param existingRecord
   *          The local <code>Record</code> to replace.
   * @throws RemoteException
   */
  protected void replaceExistingRecordWithRegularRecord(Record toStore, Record existingRecord)
      throws RemoteException {
    if (existingRecord.deleted) {
      // Need two database operations -- purge from deleted table, insert into regular table.
      deleteExistingRecord(existingRecord);
      insertNewRegularRecord(toStore);
      return;
    }

    final ContentValues cv = contentValuesForRegularRecord(toStore);
    int updated = formsProvider.update(FORM_HISTORY_CONTENT_URI, cv, GUID_IS, new String[] { existingRecord.guid });
    if (updated != 1) {
      Logger.warn(LOG_TAG, "Expected to update 1 record with guid " + existingRecord.guid + " but updated " + updated + " records.");
    }
  }

  @Override
  public void store(Record rawRecord) throws NoStoreDelegateException {
    if (storeDelegate == null) {
      Logger.warn(LOG_TAG, "No store delegate.");
      throw new NoStoreDelegateException();
    }
    if (rawRecord == null) {
      Logger.error(LOG_TAG, "Record sent to store was null");
      throw new IllegalArgumentException("Null record passed to FormHistoryRepositorySession.store().");
    }
    if (!(rawRecord instanceof FormHistoryRecord)) {
      Logger.error(LOG_TAG, "Can't store anything but a FormHistoryRecord");
      throw new IllegalArgumentException("Non-FormHistoryRecord passed to FormHistoryRepositorySession.store().");
    }
    final FormHistoryRecord record = (FormHistoryRecord) rawRecord;

    Runnable command = new Runnable() {
      @Override
      public void run() {
        if (!isActive()) {
          Logger.warn(LOG_TAG, "FormHistoryRepositorySession is inactive. Store failing.");
          storeDelegate.onRecordStoreFailed(new InactiveSessionException(), record.guid);
          return;
        }

        // TODO: lift these into the session.
        // Temporary: this matches prior syncing semantics, in which only
        // the relationship between the local and remote record is considered.
        // In the future we'll track these two timestamps and use them to
        // determine which records have changed, and thus process incoming
        // records more efficiently.
        long lastLocalRetrieval  = 0;      // lastSyncTimestamp?
        long lastRemoteRetrieval = 0;      // TODO: adjust for clock skew.
        boolean remotelyModified = record.lastModified > lastRemoteRetrieval;

        Record existingRecord;
        try {
          // GUID matching only: deleted records don't have a payload with which to search.
          existingRecord = findExistingRecordByGuid(record.guid);
          if (record.deleted) {
            if (existingRecord == null) {
              // We're done. Don't bother with a callback. That can change later
              // if we want it to.
              Logger.trace(LOG_TAG, "Incoming record " + record.guid + " is deleted, and no local version. Bye!");
              return;
            }

            if (existingRecord.deleted) {
              Logger.trace(LOG_TAG, "Local record already deleted. Purging local.");
              deleteExistingRecord(existingRecord);
              return;
            }

            // Which one wins?
            if (!remotelyModified) {
              Logger.trace(LOG_TAG, "Ignoring deleted record from the past.");
              return;
            }

            boolean locallyModified = existingRecord.lastModified > lastLocalRetrieval;
            if (!locallyModified) {
              Logger.trace(LOG_TAG, "Remote modified, local not. Deleting.");
              deleteExistingRecord(existingRecord);
              trackRecord(record);
              storeDelegate.onRecordStoreSucceeded(record.guid);
              return;
            }

            Logger.trace(LOG_TAG, "Both local and remote records have been modified.");
            if (record.lastModified > existingRecord.lastModified) {
              Logger.trace(LOG_TAG, "Remote is newer, and deleted. Purging local.");
              deleteExistingRecord(existingRecord);
              trackRecord(record);
              storeDelegate.onRecordStoreSucceeded(record.guid);
              return;
            }

            Logger.trace(LOG_TAG, "Remote is older, local is not deleted. Ignoring.");
            return;
          }
          // End deletion logic.

          // Now we're processing a non-deleted incoming record.
          if (existingRecord == null) {
            Logger.trace(LOG_TAG, "Looking up match for record " + record.guid);
            existingRecord = findExistingRecordByPayload(record);
          }

          if (existingRecord == null) {
            // The record is new.
            Logger.trace(LOG_TAG, "No match. Inserting.");
            insertNewRegularRecord(record);
            trackRecord(record);
            storeDelegate.onRecordStoreSucceeded(record.guid);
            return;
          }

          // We found a local duplicate.
          Logger.trace(LOG_TAG, "Incoming record " + record.guid + " dupes to local record " + existingRecord.guid);

          if (!RepoUtils.stringsEqual(record.guid, existingRecord.guid)) {
            // We found a local record that does NOT have the same GUID -- keep the server's version.
            Logger.trace(LOG_TAG, "Remote guid different from local guid. Storing to keep remote guid.");
            replaceExistingRecordWithRegularRecord(record, existingRecord);
            trackRecord(record);
            storeDelegate.onRecordStoreSucceeded(record.guid);
            return;
          }

          // We found a local record that does have the same GUID -- check modification times.
          boolean locallyModified = existingRecord.lastModified > lastLocalRetrieval;
          if (!locallyModified) {
            Logger.trace(LOG_TAG, "Remote modified, local not. Storing.");
            replaceExistingRecordWithRegularRecord(record, existingRecord);
            trackRecord(record);
            storeDelegate.onRecordStoreSucceeded(record.guid);
            return;
          }

          Logger.trace(LOG_TAG, "Both local and remote records have been modified.");
          if (record.lastModified > existingRecord.lastModified) {
            Logger.trace(LOG_TAG, "Remote is newer, and not deleted. Storing.");
            replaceExistingRecordWithRegularRecord(record, existingRecord);
            trackRecord(record);
            storeDelegate.onRecordStoreSucceeded(record.guid);
            return;
          }

          Logger.trace(LOG_TAG, "Remote is older, local is not deleted. Ignoring.");
        } catch (Exception e) {
          Logger.error(LOG_TAG, "Store failed for " + record.guid, e);
          storeDelegate.onRecordStoreFailed(e, record.guid);
          return;
        }
      }
    };

    storeWorkQueue.execute(command);
  }

  /**
   * Purge all data from the underlying databases.
   */
  public static void purgeDatabases(ContentProviderClient formsProvider)
      throws RemoteException {
    formsProvider.delete(FORM_HISTORY_CONTENT_URI, null, null);
    formsProvider.delete(DELETED_FORM_HISTORY_CONTENT_URI, null, null);
  }

  @Override
  public void wipe(final RepositorySessionWipeDelegate delegate) {
    Runnable command = new Runnable() {
      @Override
      public void run() {
        if (!isActive()) {
          delegate.onWipeFailed(new InactiveSessionException());
          return;
        }

        try {
          Logger.debug(LOG_TAG, "Wiping form history and deleted form history...");
          purgeDatabases(formsProvider);
          Logger.debug(LOG_TAG, "Wiping form history and deleted form history... DONE");
        } catch (Exception e) {
          delegate.onWipeFailed(e);
          return;
        }

        delegate.onWipeSucceeded();
      }
    };
    storeWorkQueue.execute(command);
  }
}
