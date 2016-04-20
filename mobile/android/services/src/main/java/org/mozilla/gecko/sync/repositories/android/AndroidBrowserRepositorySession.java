/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.repositories.android;

import java.util.ArrayList;

import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.sync.repositories.InactiveSessionException;
import org.mozilla.gecko.sync.repositories.InvalidRequestException;
import org.mozilla.gecko.sync.repositories.InvalidSessionTransitionException;
import org.mozilla.gecko.sync.repositories.MultipleRecordsForGuidException;
import org.mozilla.gecko.sync.repositories.NoGuidForIdException;
import org.mozilla.gecko.sync.repositories.NoStoreDelegateException;
import org.mozilla.gecko.sync.repositories.NullCursorException;
import org.mozilla.gecko.sync.repositories.ParentNotFoundException;
import org.mozilla.gecko.sync.repositories.ProfileDatabaseException;
import org.mozilla.gecko.sync.repositories.RecordFilter;
import org.mozilla.gecko.sync.repositories.Repository;
import org.mozilla.gecko.sync.repositories.StoreTrackingRepositorySession;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionBeginDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionFetchRecordsDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionFinishDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionGuidsSinceDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionWipeDelegate;
import org.mozilla.gecko.sync.repositories.domain.Record;

import android.content.ContentUris;
import android.database.Cursor;
import android.net.Uri;
import android.util.SparseArray;

/**
 * You'll notice that all delegate calls *either*:
 *
 * - request a deferred delegate with the appropriate work queue, then
 *   make the appropriate call, or
 * - create a Runnable which makes the appropriate call, and pushes it
 *   directly into the appropriate work queue.
 *
 * This is to ensure that all delegate callbacks happen off the current
 * thread. This provides lock safety (we don't enter another method that
 * might try to take a lock already taken in our caller), and ensures
 * that operations take place off the main thread.
 *
 * Don't do both -- the two approaches are equivalent -- and certainly
 * don't do neither unless you know what you're doing!
 *
 * Similarly, all store calls go through the appropriate store queue. This
 * ensures that store() and storeDone() consequences occur before-after.
 *
 * @author rnewman
 *
 */
public abstract class AndroidBrowserRepositorySession extends StoreTrackingRepositorySession {
  public static final String LOG_TAG = "BrowserRepoSession";

  protected AndroidBrowserRepositoryDataAccessor dbHelper;

  /**
   * In order to reconcile the "same record" with two *different* GUIDs (for
   * example, the same bookmark created by two different clients), we maintain a
   * mapping for each local record from a "record string" to
   * "local record GUID".
   * <p>
   * The "record string" above is a "record identifying unique key" produced by
   * <code>buildRecordString</code>.
   * <p>
   * Since we hash each "record string", this map may produce a false positive.
   * In this case, we search the database for a matching record explicitly using
   * <code>findByRecordString</code>.
   */
  protected SparseArray<String> recordToGuid;

  public AndroidBrowserRepositorySession(Repository repository) {
    super(repository);
  }

  /**
   * Retrieve a record from a cursor. Act as if we don't know the final contents of
   * the record: for example, a folder's child array might change.
   *
   * Return null if this record should not be processed.
   *
   * @throws NoGuidForIdException
   * @throws NullCursorException
   * @throws ParentNotFoundException
   */
  protected abstract Record retrieveDuringStore(Cursor cur) throws NoGuidForIdException, NullCursorException, ParentNotFoundException;

  /**
   * Retrieve a record from a cursor. Ensure that the contents of the database are
   * updated to match the record that we're constructing: for example, the children
   * of a folder might be repositioned as we generate the folder's record.
   *
   * @throws NoGuidForIdException
   * @throws NullCursorException
   * @throws ParentNotFoundException
   */
  protected abstract Record retrieveDuringFetch(Cursor cur) throws NoGuidForIdException, NullCursorException, ParentNotFoundException;

  /**
   * Override this to allow records to be skipped during insertion.
   *
   * For example, a session subclass might skip records of an unsupported type.
   */
  @SuppressWarnings("static-method")
  public boolean shouldIgnore(Record record) {
    return false;
  }

  /**
   * Perform any necessary transformation of a record prior to searching by
   * any field other than GUID.
   *
   * Example: translating remote folder names into local names.
   */
  @SuppressWarnings("static-method")
  protected void fixupRecord(Record record) {
    return;
  }

  /**
   * Override in subclass to implement record extension.
   *
   * Populate any fields of the record that are expensive to calculate,
   * prior to reconciling.
   *
   * Example: computing children arrays.
   *
   * Return null if this record should not be processed.
   *
   * @param record
   *        The record to transform. Can be null.
   * @return The transformed record. Can be null.
   * @throws NullCursorException
   */
  @SuppressWarnings("static-method")
  protected Record transformRecord(Record record) throws NullCursorException {
    return record;
  }

  @Override
  public void begin(RepositorySessionBeginDelegate delegate) throws InvalidSessionTransitionException {
    RepositorySessionBeginDelegate deferredDelegate = delegate.deferredBeginDelegate(delegateQueue);
    super.sharedBegin();

    try {
      // We do this check here even though it results in one extra call to the DB
      // because if we didn't, we have to do a check on every other call since there
      // is no way of knowing which call would be hit first.
      checkDatabase();
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
  public void finish(RepositorySessionFinishDelegate delegate) throws InactiveSessionException {
    dbHelper = null;
    recordToGuid = null;
    super.finish(delegate);
  }

  /**
   * Produce a "record string" (record identifying unique key).
   *
   * @param record
   *          the <code>Record</code> to identify.
   * @return a <code>String</code> instance.
   */
  protected abstract String buildRecordString(Record record);

  protected void checkDatabase() throws ProfileDatabaseException, NullCursorException {
    Logger.debug(LOG_TAG, "BEGIN: checking database.");
    try {
      dbHelper.fetch(new String[] { "none" }).close();
      Logger.debug(LOG_TAG, "END: checking database.");
    } catch (NullPointerException e) {
      throw new ProfileDatabaseException(e);
    }
  }

  @Override
  public void guidsSince(long timestamp, RepositorySessionGuidsSinceDelegate delegate) {
    GuidsSinceRunnable command = new GuidsSinceRunnable(timestamp, delegate);
    delegateQueue.execute(command);
  }

  class GuidsSinceRunnable implements Runnable {

    private final RepositorySessionGuidsSinceDelegate delegate;
    private final long                                timestamp;

    public GuidsSinceRunnable(long timestamp,
                              RepositorySessionGuidsSinceDelegate delegate) {
      this.timestamp = timestamp;
      this.delegate = delegate;
    }

    @Override
    public void run() {
      if (!isActive()) {
        delegate.onGuidsSinceFailed(new InactiveSessionException(null));
        return;
      }

      Cursor cur;
      try {
        cur = dbHelper.getGUIDsSince(timestamp);
      } catch (Exception e) {
        delegate.onGuidsSinceFailed(e);
        return;
      }

      ArrayList<String> guids;
      try {
        if (!cur.moveToFirst()) {
          delegate.onGuidsSinceSucceeded(new String[] {});
          return;
        }
        guids = new ArrayList<String>();
        while (!cur.isAfterLast()) {
          guids.add(RepoUtils.getStringFromCursor(cur, "guid"));
          cur.moveToNext();
        }
      } finally {
        Logger.debug(LOG_TAG, "Closing cursor after guidsSince.");
        cur.close();
      }

      String guidsArray[] = new String[guids.size()];
      guids.toArray(guidsArray);
      delegate.onGuidsSinceSucceeded(guidsArray);
    }
  }

  @Override
  public void fetch(String[] guids,
                    RepositorySessionFetchRecordsDelegate delegate) throws InactiveSessionException {
    FetchRunnable command = new FetchRunnable(guids, now(), null, delegate);
    executeDelegateCommand(command);
  }

  abstract class FetchingRunnable implements Runnable {
    protected final RepositorySessionFetchRecordsDelegate delegate;

    public FetchingRunnable(RepositorySessionFetchRecordsDelegate delegate) {
      this.delegate = delegate;
    }

    protected void fetchFromCursor(Cursor cursor, RecordFilter filter, long end) {
      Logger.debug(LOG_TAG, "Fetch from cursor:");
      try {
        try {
          if (!cursor.moveToFirst()) {
            delegate.onFetchCompleted(end);
            return;
          }
          while (!cursor.isAfterLast()) {
            Record r = retrieveDuringFetch(cursor);
            if (r != null) {
              if (filter == null || !filter.excludeRecord(r)) {
                Logger.trace(LOG_TAG, "Processing record " + r.guid);
                delegate.onFetchedRecord(transformRecord(r));
              } else {
                Logger.debug(LOG_TAG, "Skipping filtered record " + r.guid);
              }
            }
            cursor.moveToNext();
          }
          delegate.onFetchCompleted(end);
        } catch (NoGuidForIdException e) {
          Logger.warn(LOG_TAG, "No GUID for ID.", e);
          delegate.onFetchFailed(e, null);
        } catch (Exception e) {
          Logger.warn(LOG_TAG, "Exception in fetchFromCursor.", e);
          delegate.onFetchFailed(e, null);
          return;
        }
      } finally {
        Logger.trace(LOG_TAG, "Closing cursor after fetch.");
        cursor.close();
      }
    }
  }

  public class FetchRunnable extends FetchingRunnable {
    private final String[] guids;
    private final long     end;
    private final RecordFilter filter;

    public FetchRunnable(String[] guids,
                         long end,
                         RecordFilter filter,
                         RepositorySessionFetchRecordsDelegate delegate) {
      super(delegate);
      this.guids  = guids;
      this.end    = end;
      this.filter = filter;
    }

    @Override
    public void run() {
      if (!isActive()) {
        delegate.onFetchFailed(new InactiveSessionException(null), null);
        return;
      }

      if (guids == null || guids.length < 1) {
        Logger.error(LOG_TAG, "No guids sent to fetch");
        delegate.onFetchFailed(new InvalidRequestException(null), null);
        return;
      }

      try {
        Cursor cursor = dbHelper.fetch(guids);
        this.fetchFromCursor(cursor, filter, end);
      } catch (NullCursorException e) {
        delegate.onFetchFailed(e, null);
      }
    }
  }

  @Override
  public void fetchSince(long timestamp,
                         RepositorySessionFetchRecordsDelegate delegate) {
    if (this.storeTracker == null) {
      throw new IllegalStateException("Store tracker not yet initialized!");
    }

    Logger.debug(LOG_TAG, "Running fetchSince(" + timestamp + ").");
    FetchSinceRunnable command = new FetchSinceRunnable(timestamp, now(), this.storeTracker.getFilter(), delegate);
    delegateQueue.execute(command);
  }

  class FetchSinceRunnable extends FetchingRunnable {
    private final long since;
    private final long end;
    private final RecordFilter filter;

    public FetchSinceRunnable(long since,
                              long end,
                              RecordFilter filter,
                              RepositorySessionFetchRecordsDelegate delegate) {
      super(delegate);
      this.since  = since;
      this.end    = end;
      this.filter = filter;
    }

    @Override
    public void run() {
      if (!isActive()) {
        delegate.onFetchFailed(new InactiveSessionException(null), null);
        return;
      }

      try {
        Cursor cursor = dbHelper.fetchSince(since);
        this.fetchFromCursor(cursor, filter, end);
      } catch (NullCursorException e) {
        delegate.onFetchFailed(e, null);
        return;
      }
    }
  }

  @Override
  public void fetchAll(RepositorySessionFetchRecordsDelegate delegate) {
    this.fetchSince(0, delegate);
  }

  protected int storeCount = 0;

  @Override
  public void store(final Record record) throws NoStoreDelegateException {
    if (delegate == null) {
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
    Runnable command = new Runnable() {

      @Override
      public void run() {
        if (!isActive()) {
          Logger.warn(LOG_TAG, "AndroidBrowserRepositorySession is inactive. Store failing.");
          delegate.onRecordStoreFailed(new InactiveSessionException(null), record.guid);
          return;
        }

        // Check that the record is a valid type.
        // Fennec only supports bookmarks and folders. All other types of records,
        // including livemarks and queries, are simply ignored.
        // See Bug 708149. This might be resolved by Fennec changing its database
        // schema, or by Sync storing non-applied records in its own private database.
        if (shouldIgnore(record)) {
          Logger.debug(LOG_TAG, "Ignoring record " + record.guid);

          // Don't throw: we don't want to abort the entire sync when we get a livemark!
          // delegate.onRecordStoreFailed(new InvalidBookmarkTypeException(null));
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
          existingRecord = retrieveByGUIDDuringStore(record.guid);
          if (record.deleted) {
            if (existingRecord == null) {
              // We're done. Don't bother with a callback. That can change later
              // if we want it to.
              trace("Incoming record " + record.guid + " is deleted, and no local version. Bye!");
              return;
            }

            if (existingRecord.deleted) {
              trace("Local record already deleted. Bye!");
              return;
            }

            // Which one wins?
            if (!remotelyModified) {
              trace("Ignoring deleted record from the past.");
              return;
            }

            boolean locallyModified = existingRecord.lastModified > lastLocalRetrieval;
            if (!locallyModified) {
              trace("Remote modified, local not. Deleting.");
              storeRecordDeletion(record, existingRecord);
              return;
            }

            trace("Both local and remote records have been modified.");
            if (record.lastModified > existingRecord.lastModified) {
              trace("Remote is newer, and deleted. Deleting local.");
              storeRecordDeletion(record, existingRecord);
              return;
            }

            trace("Remote is older, local is not deleted. Ignoring.");
            return;
          }
          // End deletion logic.

          // Now we're processing a non-deleted incoming record.
          // Apply any changes we need in order to correctly find existing records.
          fixupRecord(record);

          if (existingRecord == null) {
            trace("Looking up match for record " + record.guid);
            existingRecord = findExistingRecord(record);
          }

          if (existingRecord == null) {
            // The record is new.
            trace("No match. Inserting.");
            insert(record);
            return;
          }

          // We found a local dupe.
          trace("Incoming record " + record.guid + " dupes to local record " + existingRecord.guid);

          // Populate more expensive fields prior to reconciling.
          existingRecord = transformRecord(existingRecord);
          Record toStore = reconcileRecords(record, existingRecord, lastRemoteRetrieval, lastLocalRetrieval);

          if (toStore == null) {
            Logger.debug(LOG_TAG, "Reconciling returned null. Not inserting a record.");
            return;
          }

          // TODO: pass in timestamps?

          // This section of code will only run if the incoming record is not
          // marked as deleted, so we never want to just drop ours from the database:
          // we need to upload it later.
          // Allowing deleted items to propagate through `replace` allows normal
          // logging and side-effects to occur, and is no more expensive than simply
          // bumping the modified time.
          Logger.debug(LOG_TAG, "Replacing existing " + existingRecord.guid +
                       (toStore.deleted ? " with deleted record " : " with record ") +
                       toStore.guid);
          Record replaced = replace(toStore, existingRecord);

          // Note that we don't track records here; deciding that is the job
          // of reconcileRecords.
          Logger.debug(LOG_TAG, "Calling delegate callback with guid " + replaced.guid +
                                "(" + replaced.androidID + ")");
          delegate.onRecordStoreSucceeded(replaced.guid);
          return;

        } catch (MultipleRecordsForGuidException e) {
          Logger.error(LOG_TAG, "Multiple records returned for given guid: " + record.guid);
          delegate.onRecordStoreFailed(e, record.guid);
          return;
        } catch (NoGuidForIdException e) {
          Logger.error(LOG_TAG, "Store failed for " + record.guid, e);
          delegate.onRecordStoreFailed(e, record.guid);
          return;
        } catch (Exception e) {
          Logger.error(LOG_TAG, "Store failed for " + record.guid, e);
          delegate.onRecordStoreFailed(e, record.guid);
          return;
        }
      }
    };
    storeWorkQueue.execute(command);
  }

  /**
   * Process a request for deletion of a record.
   * Neither argument will ever be null.
   *
   * @param record the incoming record. This will be mostly blank, given that it's a deletion.
   * @param existingRecord the existing record. Use this to decide how to process the deletion.
   */
  protected void storeRecordDeletion(final Record record, final Record existingRecord) {
    // TODO: we ought to mark the record as deleted rather than purging it,
    // in order to support syncing to multiple destinations. Bug 722607.
    dbHelper.purgeGuid(record.guid);
    delegate.onRecordStoreSucceeded(record.guid);
  }

  protected void insert(Record record) throws NoGuidForIdException, NullCursorException, ParentNotFoundException {
    Record toStore = prepareRecord(record);
    Uri recordURI = dbHelper.insert(toStore);
    if (recordURI == null) {
      throw new NullCursorException(new RuntimeException("Got null URI inserting record with guid " + record.guid));
    }
    toStore.androidID = ContentUris.parseId(recordURI);

    updateBookkeeping(toStore);
    trackRecord(toStore);
    delegate.onRecordStoreSucceeded(toStore.guid);

    Logger.debug(LOG_TAG, "Inserted record with guid " + toStore.guid + " as androidID " + toStore.androidID);
  }

  protected Record replace(Record newRecord, Record existingRecord) throws NoGuidForIdException, NullCursorException, ParentNotFoundException {
    Record toStore = prepareRecord(newRecord);

    // newRecord should already have suitable androidID and guid.
    dbHelper.update(existingRecord.guid, toStore);
    updateBookkeeping(toStore);
    Logger.debug(LOG_TAG, "replace() returning record " + toStore.guid);
    return toStore;
  }

  /**
   * Retrieve a record from the store by GUID, without writing unnecessarily to the
   * database.
   *
   * @throws NoGuidForIdException
   * @throws NullCursorException
   * @throws ParentNotFoundException
   * @throws MultipleRecordsForGuidException
   */
  protected Record retrieveByGUIDDuringStore(String guid) throws
                                             NoGuidForIdException,
                                             NullCursorException,
                                             ParentNotFoundException,
                                             MultipleRecordsForGuidException {
    Cursor cursor = dbHelper.fetch(new String[] { guid });
    try {
      if (!cursor.moveToFirst()) {
        return null;
      }

      Record r = retrieveDuringStore(cursor);

      cursor.moveToNext();
      if (cursor.isAfterLast()) {
        // Got one record!
        return r; // Not transformed.
      }

      // More than one. Oh dear.
      throw (new MultipleRecordsForGuidException(null));
    } finally {
      cursor.close();
    }
  }

  /**
   * Attempt to find an equivalent record through some means other than GUID.
   *
   * @param record
   *        The record for which to search.
   * @return
   *        An equivalent Record object, or null if none is found.
   *
   * @throws MultipleRecordsForGuidException
   * @throws NoGuidForIdException
   * @throws NullCursorException
   * @throws ParentNotFoundException
   */
  protected Record findExistingRecord(Record record) throws MultipleRecordsForGuidException,
    NoGuidForIdException, NullCursorException, ParentNotFoundException {

    Logger.debug(LOG_TAG, "Finding existing record for incoming record with GUID " + record.guid);
    String recordString = buildRecordString(record);
    if (recordString == null) {
      Logger.debug(LOG_TAG, "No record string for incoming record " + record.guid);
      return null;
    }

    if (Logger.LOG_PERSONAL_INFORMATION) {
      Logger.pii(LOG_TAG, "Searching with record string " + recordString);
    } else {
      Logger.debug(LOG_TAG, "Searching with record string.");
    }
    String guid = getGuidForString(recordString);
    if (guid == null) {
      Logger.debug(LOG_TAG, "Failed to find existing record for " + record.guid);
      return null;
    }

    // Our map contained a match, but it could be a false positive. Since
    // computed record string is supposed to be a unique key, we can easily
    // verify our positive.
    Logger.debug(LOG_TAG, "Found one. Checking stored record.");
    Record stored = retrieveByGUIDDuringStore(guid);
    String storedRecordString = buildRecordString(record);
    if (recordString.equals(storedRecordString)) {
      Logger.debug(LOG_TAG, "Existing record matches incoming record.  Returning existing record.");
      return stored;
    }

    // Oh no, we got a false positive! (This should be *very* rare --
    // essentially, we got a hash collision.) Search the DB for this record
    // explicitly by hand.
    Logger.debug(LOG_TAG, "Existing record does not match incoming record.  Trying to find record by record string.");
    return findByRecordString(recordString);
  }

  protected String getGuidForString(String recordString) throws NoGuidForIdException, NullCursorException, ParentNotFoundException {
    if (recordToGuid == null) {
      createRecordToGuidMap();
    }
    return recordToGuid.get(recordString.hashCode());
  }

  protected void createRecordToGuidMap() throws NoGuidForIdException, NullCursorException, ParentNotFoundException {
    Logger.info(LOG_TAG, "BEGIN: creating record -> GUID map.");
    recordToGuid = new SparseArray<String>();

    // TODO: we should be able to do this entire thing with string concatenations within SQL.
    // Also consider whether it's better to fetch and process every record in the DB into
    // memory, or run a query per record to do the same thing.
    Cursor cur = dbHelper.fetchAll();
    try {
      if (!cur.moveToFirst()) {
        return;
      }
      while (!cur.isAfterLast()) {
        Record record = retrieveDuringStore(cur);
        if (record != null) {
          final String recordString = buildRecordString(record);
          if (recordString != null) {
            recordToGuid.put(recordString.hashCode(), record.guid);
          }
        }
        cur.moveToNext();
      }
    } finally {
      cur.close();
    }
    Logger.info(LOG_TAG, "END: creating record -> GUID map.");
  }

  /**
   * Search the local database for a record with the same "record string".
   * <p>
   * We expect to do this only in the unlikely event of a hash
   * collision, so we iterate the database completely.  Since we want
   * to include information about the parents of bookmarks, it is
   * difficult to do better purely using the
   * <code>ContentProvider</code> interface.
   *
   * @param recordString
   *          the "record string" to search for; must be n
   * @return a <code>Record</code> with the same "record string", or
   *         <code>null</code> if none is present.
   * @throws ParentNotFoundException
   * @throws NullCursorException
   * @throws NoGuidForIdException
   */
  protected Record findByRecordString(String recordString) throws NoGuidForIdException, NullCursorException, ParentNotFoundException {
    Cursor cur = dbHelper.fetchAll();
    try {
      if (!cur.moveToFirst()) {
        return null;
      }
      while (!cur.isAfterLast()) {
        Record record = retrieveDuringStore(cur);
        if (record != null) {
          final String storedRecordString = buildRecordString(record);
          if (recordString.equals(storedRecordString)) {
            return record;
          }
        }
        cur.moveToNext();
      }
      return null;
    } finally {
      cur.close();
    }
  }

  public void putRecordToGuidMap(String recordString, String guid) throws NoGuidForIdException, NullCursorException, ParentNotFoundException {
    if (recordString == null) {
      return;
    }

    if (recordToGuid == null) {
      createRecordToGuidMap();
    }
    recordToGuid.put(recordString.hashCode(), guid);
  }

  protected abstract Record prepareRecord(Record record);

  protected void updateBookkeeping(Record record) throws NoGuidForIdException,
                                                 NullCursorException,
                                                 ParentNotFoundException {
    putRecordToGuidMap(buildRecordString(record), record.guid);
  }

  protected WipeRunnable getWipeRunnable(RepositorySessionWipeDelegate delegate) {
    return new WipeRunnable(delegate);
  }

  @Override
  public void wipe(RepositorySessionWipeDelegate delegate) {
    Runnable command = getWipeRunnable(delegate);
    storeWorkQueue.execute(command);
  }

  class WipeRunnable implements Runnable {
    protected RepositorySessionWipeDelegate delegate;

    public WipeRunnable(RepositorySessionWipeDelegate delegate) {
      this.delegate = delegate;
    }

    @Override
    public void run() {
      if (!isActive()) {
        delegate.onWipeFailed(new InactiveSessionException(null));
        return;
      }
      dbHelper.wipe();
      delegate.onWipeSucceeded();
    }
  }

  // For testing purposes.
  public AndroidBrowserRepositoryDataAccessor getDBHelper() {
    return dbHelper;
  }
}
