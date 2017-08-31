/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.repositories.android;

import android.database.Cursor;
import android.util.SparseArray;

import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.sync.repositories.InactiveSessionException;
import org.mozilla.gecko.sync.repositories.InvalidRequestException;
import org.mozilla.gecko.sync.repositories.MultipleRecordsForGuidException;
import org.mozilla.gecko.sync.repositories.NoGuidForIdException;
import org.mozilla.gecko.sync.repositories.NullCursorException;
import org.mozilla.gecko.sync.repositories.ParentNotFoundException;
import org.mozilla.gecko.sync.repositories.ProfileDatabaseException;
import org.mozilla.gecko.sync.repositories.RecordFilter;
import org.mozilla.gecko.sync.repositories.StoreTrackingRepositorySession;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionFetchRecordsDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionStoreDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionWipeDelegate;
import org.mozilla.gecko.sync.repositories.domain.Record;


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
/* package-private */ abstract class SessionHelper {
    private final static String LOG_TAG = "SessionHelper";

    protected final StoreTrackingRepositorySession session;

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
    private SparseArray<String> recordToGuid;

    /* package-private */ final DataAccessor dbHelper;

    /* package-private */ SessionHelper(StoreTrackingRepositorySession session, DataAccessor dbHelper) {
        this.session = session;
        this.dbHelper = dbHelper;
    }

    /**
     * Retrieve a record from a cursor. Ensure that the contents of the database are
     * updated to match the record that we're constructing: for example, the children
     * of a folder might be repositioned as we generate the folder's record.
     *
     * @throws NoGuidForIdException
     * @throws NullCursorException
     * @throws ParentNotFoundException
     */
    abstract Record retrieveDuringFetch(Cursor cursor)throws NoGuidForIdException, NullCursorException, ParentNotFoundException;
    abstract Runnable getWipeRunnable(RepositorySessionWipeDelegate delegate);

    /**
     * Perform any necessary transformation of a record prior to searching by
     * any field other than GUID.
     *
     * Example: translating remote folder names into local names.
     */
    abstract void fixupRecord(Record record);

    /**
     * Override this to allow records to be skipped during insertion.
     *
     * For example, a session subclass might skip records of an unsupported type.
     */
    abstract boolean shouldIgnore(Record record);

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
    abstract Record retrieveDuringStore(Cursor cursor) throws NoGuidForIdException, NullCursorException, ParentNotFoundException;
    abstract void storeRecordDeletion(RepositorySessionStoreDelegate storeDelegate, Record record, Record existingRecord);

    /**
     * Produce a "record string" (record identifying unique key).
     *
     * @param record
     *          the <code>Record</code> to identify.
     * @return a <code>String</code> instance.
     */
    abstract String buildRecordString(Record record);

    abstract Record processBeforeInsertion(Record toProcess);

    abstract void insert(RepositorySessionStoreDelegate delegate, Record record)
            throws NoGuidForIdException, NullCursorException,ParentNotFoundException;

    abstract Record reconcileRecords(final Record remoteRecord,
                                     final Record localRecord,
                                     final long lastRemoteRetrieval,
                                     final long lastLocalRetrieval);

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
     */
    abstract Record transformRecord(Record record);

    abstract Record prepareRecord(Record record);

    abstract void doBegin() throws NullCursorException;

    abstract Runnable getStoreDoneRunnable(RepositorySessionStoreDelegate delegate);

    /* package-private */ void updateBookkeeping(Record record)
            throws NoGuidForIdException, NullCursorException, ParentNotFoundException {
        putRecordToGuidMap(buildRecordString(record), record.guid);
    }

    abstract boolean doReplaceRecord(Record toStore, Record existingRecord, RepositorySessionStoreDelegate delegate)
            throws NoGuidForIdException, NullCursorException, ParentNotFoundException;

    abstract boolean isLocallyModified(Record record);

    void checkDatabase() throws ProfileDatabaseException, NullCursorException {
        Logger.debug(LOG_TAG, "BEGIN: checking database.");
        try {
            dbHelper.fetch(new String[] { "none" }).close();
            Logger.debug(LOG_TAG, "END: checking database.");
        } catch (NullPointerException e) {
            throw new ProfileDatabaseException(e);
        }
    }

    void finish() {
        // TODO are we paranoid that a reference might persist for too long once we're done with it?
        recordToGuid = null;
    }

    private boolean shouldReconcileRecords(final Record remoteRecord,
                                           final Record localRecord) {
        return session.shouldReconcileRecords(remoteRecord, localRecord);
    }

    private void putRecordToGuidMap(String recordString, String guid)
            throws NoGuidForIdException, NullCursorException, ParentNotFoundException {
        if (recordString == null) {
            return;
        }

        if (recordToGuid == null) {
            createRecordToGuidMap();
        }
        recordToGuid.put(recordString.hashCode(), guid);
    }

    /* package-private */ Runnable getStoreRunnable(final Record record, final RepositorySessionStoreDelegate storeDelegate) {
        return new Runnable() {
            @Override
            public void run() {
                if (!session.isActive()) {
                    Logger.warn(LOG_TAG, "AndroidBrowserRepositorySession is inactive. Store failing.");
                    storeDelegate.onRecordStoreFailed(new InactiveSessionException(), record.guid);
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


                // Temporary (written some 5 years ago, nothing is so permanent as the temporary):
                // - this matches prior syncing semantics, in which only
                // - the relationship between the local and remote record is considered.
                // In the future we should be performing a three-way merge of record sets.
                long lastLocalRetrieval  = 0;      // lastSyncTimestamp?
                long lastRemoteRetrieval = 0;      // TODO: adjust for clock skew.
                boolean remotelyModified = record.lastModified > lastRemoteRetrieval;

                Record existingRecord;
                try {
                    // For versioned records, we may attempt this process multiple times.
                    // This happens because when we merge records and perform an actual database update,
                    // we do so while also asserting that the record we're trying to overwrite did not change
                    // since we computed its merged version. If it's determined that there was a race and record
                    // did change, we attempt this whole process again: record might have been changed in any way,
                    // and so we check for deletions, re-compute merged version, etc.
                    // To stay on the safe side, we set a maximum number of attempts, and declare failure once
                    // we get exhaust our options.
                    // It is expected that this while loop will only execute once most of the time.
                    final int maxReconcileAttempts = 10;
                    int reconcileAttempt = 1;
                    do {
                        // GUID matching only: deleted records don't have a payload with which to search.
                        existingRecord = retrieveByGUIDDuringStore(record.guid);
                        if (record.deleted) {
                            if (existingRecord == null) {
                                // We're done. Don't bother with a callback. That can change later
                                // if we want it to.
                                trace("Incoming record " + record.guid + " is deleted, and no local version. Bye!");
                                break;
                            }

                            if (existingRecord.deleted) {
                                trace("Local record already deleted. Bye!");
                                break;
                            }

                            // Which one wins?
                            if (!remotelyModified) {
                                trace("Ignoring deleted record from the past.");
                                break;
                            }

                            if (!isLocallyModified(existingRecord)) {
                                trace("Remote modified, local not. Deleting.");
                                storeRecordDeletion(storeDelegate, record, existingRecord);
                                break;
                            }

                            trace("Both local and remote records have been modified.");
                            if (record.lastModified > existingRecord.lastModified) {
                                trace("Remote is newer, and deleted. Deleting local.");
                                storeRecordDeletion(storeDelegate, record, existingRecord);
                                break;
                            }

                            trace("Remote is older, local is not deleted. Ignoring.");
                            break;
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
                            insert(storeDelegate, processBeforeInsertion(record));
                            break;
                        }

                        // We found a local dupe.
                        trace("Incoming record " + record.guid + " dupes to local record " + existingRecord.guid);

                        // Populate more expensive fields prior to reconciling.
                        existingRecord = transformRecord(existingRecord);

                        if (!shouldReconcileRecords(record, existingRecord)) {
                            Logger.debug(LOG_TAG, "shouldReconcileRecords returned false. Not processing a record.");
                            break;
                        }
                        // Implementation is expected to decide if it's necessary to "track" the record.
                        Record toStore = reconcileRecords(record, existingRecord, lastRemoteRetrieval, lastLocalRetrieval);
                        Logger.debug(LOG_TAG, "Reconcile attempt #" + reconcileAttempt);

                        // This section of code will only run if the incoming record is not
                        // marked as deleted, so we never want to just drop ours from the database:
                        // we need to upload it later.
                        // Implementations are expected to ensure this happens.
                        boolean replaceSuccessful = doReplaceRecord(toStore, existingRecord, storeDelegate);

                        // Note that, clumsily, delegate success callbacks are called in doReplaceRecord.
                        if (replaceSuccessful) {
                            Logger.debug(LOG_TAG, "Successfully reconciled record on attempt #" + reconcileAttempt);
                            break;
                        }

                        // Let's try again if we failed.
                        reconcileAttempt += 1;

                    } while (reconcileAttempt <= maxReconcileAttempts);

                    // Nothing else to do but let our delegate know and log if we reached maximum
                    // reconcile attempts.
                    if (reconcileAttempt > maxReconcileAttempts) {
                        Logger.error(LOG_TAG, "Failed to store record within maximum number of allowed attempts: " + record.guid);
                        storeDelegate.onRecordStoreFailed(
                                new IllegalStateException("Reached maximum storage attempts for a record"),
                                record.guid
                        );
                    } else {
                        Logger.debug(LOG_TAG, "Stored after reconcile attempt #" + reconcileAttempt);
                    }
                } catch (MultipleRecordsForGuidException e) {
                    Logger.error(LOG_TAG, "Multiple records returned for given guid: " + record.guid);
                    storeDelegate.onRecordStoreFailed(e, record.guid);
                } catch (NoGuidForIdException e) {
                    Logger.error(LOG_TAG, "Store failed for " + record.guid, e);
                    storeDelegate.onRecordStoreFailed(e, record.guid);
                } catch (Exception e) {
                    Logger.error(LOG_TAG, "Store failed for " + record.guid, e);
                    storeDelegate.onRecordStoreFailed(e, record.guid);
                }
            }
        };
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
    private Record findExistingRecord(Record record) throws MultipleRecordsForGuidException,
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
    private Record findByRecordString(String recordString) throws NoGuidForIdException, NullCursorException, ParentNotFoundException {
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

    private String getGuidForString(String recordString) throws NoGuidForIdException, NullCursorException, ParentNotFoundException {
        if (recordToGuid == null) {
            createRecordToGuidMap();
        }
        return recordToGuid.get(recordString.hashCode());
    }

    private void createRecordToGuidMap() throws NoGuidForIdException, NullCursorException, ParentNotFoundException {
        Logger.info(LOG_TAG, "BEGIN: creating record -> GUID map.");
        recordToGuid = new SparseArray<>();

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

    /* package-private */ Runnable getFetchSinceRunnable(long since, long end, RecordFilter filter, RepositorySessionFetchRecordsDelegate delegate) {
        return new FetchSinceRunnable(since, end, filter, delegate);
    }

    /* package-private */ Runnable getFetchRunnable(String[] guids,
                                           long end,
                                           RecordFilter filter,
                                           RepositorySessionFetchRecordsDelegate delegate) {
        return new FetchRunnable(guids, end, filter, delegate);
    }

    private class FetchSinceRunnable extends FetchingRunnable {
        private final long since;
        private final long end;
        private final RecordFilter filter;

        /* package-private */ FetchSinceRunnable(long since,
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
            if (!session.isActive()) {
                delegate.onFetchFailed(new InactiveSessionException());
                return;
            }

            try {
                Cursor cursor = dbHelper.fetchSince(since);
                this.fetchFromCursor(cursor, filter, end);
            } catch (NullCursorException e) {
                delegate.onFetchFailed(e);
            }
        }
    }

    private class FetchRunnable extends FetchingRunnable {
        private final String[] guids;
        private final long     end;
        private final RecordFilter filter;

        /* package-private */ FetchRunnable(String[] guids,
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
            if (!session.isActive()) {
                delegate.onFetchFailed(new InactiveSessionException());
                return;
            }

            if (guids == null || guids.length < 1) {
                Logger.error(LOG_TAG, "No guids sent to fetch");
                delegate.onFetchFailed(new InvalidRequestException());
                return;
            }

            try {
                Cursor cursor = dbHelper.fetch(guids);
                this.fetchFromCursor(cursor, filter, end);
            } catch (NullCursorException e) {
                delegate.onFetchFailed(e);
            }
        }
    }

    /* package-private */ abstract class FetchingRunnable implements Runnable {
        protected final RepositorySessionFetchRecordsDelegate delegate;

        /* package-private */ FetchingRunnable(RepositorySessionFetchRecordsDelegate delegate) {
            this.delegate = delegate;
        }

        /* package-private */ void fetchFromCursor(Cursor cursor, RecordFilter filter, long end) {
            Logger.debug(LOG_TAG, "Fetch from cursor:");
            try {
                try {
                    if (!cursor.moveToFirst()) {
                        delegate.onFetchCompleted();
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
                    session.setLastFetchTimestamp(end);
                    delegate.onFetchCompleted();
//                } catch (NoGuidForIdException e) {
//                    Logger.warn(LOG_TAG, "No GUID for ID.", e);
//                    delegate.onFetchFailed(e);
                } catch (Exception e) {
                    Logger.warn(LOG_TAG, "Exception in fetchFromCursor.", e);
                    delegate.onFetchFailed(e);
                    return;
                }
            } finally {
                Logger.trace(LOG_TAG, "Closing cursor after fetch.");
                cursor.close();
            }
        }
    }

    /* package-private */ class WipeRunnable implements Runnable {
        protected RepositorySessionWipeDelegate delegate;

        /* package-private */ WipeRunnable(RepositorySessionWipeDelegate delegate) {
            this.delegate = delegate;
        }

        @Override
        public void run() {
            if (!session.isActive()) {
                delegate.onWipeFailed(new InactiveSessionException());
                return;
            }
            dbHelper.wipe();
            delegate.onWipeSucceeded();
        }
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
    private Record retrieveByGUIDDuringStore(String guid) throws
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

    protected static void trace(String message) {
        Logger.trace(LOG_TAG, message);
    }
}
