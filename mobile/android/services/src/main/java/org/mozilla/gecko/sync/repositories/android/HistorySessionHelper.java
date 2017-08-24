/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.repositories.android;

import android.content.ContentProviderClient;
import android.database.Cursor;
import android.os.RemoteException;
import android.support.annotation.VisibleForTesting;

import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.sync.repositories.NoGuidForIdException;
import org.mozilla.gecko.sync.repositories.NullCursorException;
import org.mozilla.gecko.sync.repositories.ParentNotFoundException;
import org.mozilla.gecko.sync.repositories.StoreTrackingRepositorySession;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionStoreDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionWipeDelegate;
import org.mozilla.gecko.sync.repositories.domain.HistoryRecord;
import org.mozilla.gecko.sync.repositories.domain.Record;

import java.util.ArrayList;

/* package-private */ class HistorySessionHelper extends SessionHelper {
    private final static String LOG_TAG = "HistorySessionHelper";

    /**
     * The number of records to queue for insertion before writing to databases.
     */
    private static final int INSERT_RECORD_THRESHOLD = 5000;
    private static final int RECENT_VISITS_LIMIT = 20;

    private final Object recordsBufferMonitor = new Object();
    private ArrayList<HistoryRecord> recordsBuffer = new ArrayList<HistoryRecord>();

    /* package-private */ HistorySessionHelper(StoreTrackingRepositorySession session, DataAccessor dbHelper) {
        super(session, dbHelper);
    }

    @Override
    /* package-private */ Record retrieveDuringFetch(Cursor cur) {
        return RepoUtils.historyFromMirrorCursor(cur);
    }

    @Override
    /* package-private */ Runnable getWipeRunnable(RepositorySessionWipeDelegate delegate) {
        return new WipeRunnable(delegate);
    }

    /**
     * Process a request for deletion of a record.
     * Neither argument will ever be null.
     *
     * @param record the incoming record. This will be mostly blank, given that it's a deletion.
     * @param existingRecord the existing record. Use this to decide how to process the deletion.
     */
    @Override
    /* package-private */ void storeRecordDeletion(RepositorySessionStoreDelegate storeDelegate, Record record, Record existingRecord) {
        dbHelper.purgeGuid(record.guid);
        storeDelegate.onRecordStoreSucceeded(record.guid);
    }

    @Override
    /* package-private */ boolean shouldIgnore(Record record) {
        return shouldIgnoreStatic(record);
    }

    @VisibleForTesting
    /* package-private */ static boolean shouldIgnoreStatic(Record record) {
        if (!(record instanceof HistoryRecord)) {
            return true;
        }
        HistoryRecord r = (HistoryRecord) record;
        return !RepoUtils.isValidHistoryURI(r.histURI);
    }

    @Override
    /* package-private */ Record retrieveDuringStore(Cursor cursor) throws NoGuidForIdException, NullCursorException, ParentNotFoundException {
        return RepoUtils.historyFromMirrorCursor(cursor);
    }

    @Override
    /* package-private */ void fixupRecord(Record record) {}

    @Override
    /* package-private */ String buildRecordString(Record record) {
        return buildRecordStringStatic(record);
    }

    @VisibleForTesting
    /* package-private */ static String buildRecordStringStatic(Record record) {
        HistoryRecord hist = (HistoryRecord) record;
        return hist.histURI;
    }

    @Override
    /* package-private */ Record processBeforeInsertion(Record toProcess) {
        return toProcess;
    }

    /**
     * Queue record for insertion, possibly flushing the queue.
     * <p>
     * Must be called on <code>storeWorkQueue</code> thread! But this is only
     * called from <code>store</code>, which is called on the queue thread.
     *
     * @param record
     *          A <code>Record</code> with a GUID that is not present locally.
     */
    /* package-private */ void insert(RepositorySessionStoreDelegate delegate, Record record) throws NoGuidForIdException, NullCursorException, ParentNotFoundException {
        enqueueNewRecord(delegate, (HistoryRecord) record);
    }

    @Override
    Record reconcileRecords(Record remoteRecord, Record localRecord, long lastRemoteRetrieval, long lastLocalRetrieval) {
        return session.reconcileRecords(remoteRecord, localRecord, lastRemoteRetrieval, lastLocalRetrieval);
    }

    @Override
    Record prepareRecord(Record record) {
        return record;
    }

    @Override
    Record transformRecord(Record record) {
        return addVisitsToRecord(record);
    }

    @Override
    void doBegin() throws NullCursorException {}

    @Override
    Runnable getStoreDoneRunnable(final RepositorySessionStoreDelegate delegate) {
        return getFlushRecordsRunnabler(delegate);
    }

    @Override
    boolean doReplaceRecord(Record toStore, Record existingRecord, RepositorySessionStoreDelegate delegate) throws NoGuidForIdException, NullCursorException, ParentNotFoundException {
        Logger.debug(LOG_TAG, "Replacing existing " + existingRecord.guid +
                (toStore.deleted ? " with deleted record " : " with record ") +
                toStore.guid);

        Record preparedToStore = prepareRecord(toStore);

        // newRecord should already have suitable androidID and guid.
        dbHelper.update(existingRecord.guid, preparedToStore);
        updateBookkeeping(preparedToStore);
        Logger.debug(LOG_TAG, "replace() returning record " + preparedToStore.guid);

        Logger.debug(LOG_TAG, "Calling delegate callback with guid " + preparedToStore.guid +
                "(" + preparedToStore.androidID + ")");
        delegate.onRecordStoreReconciled(preparedToStore.guid, existingRecord.guid, null);
        delegate.onRecordStoreSucceeded(preparedToStore.guid);

        return true;
    }

    @Override
    boolean isLocallyModified(Record record) {
        return record.lastModified > session.getLastSyncTimestamp();
    }

    Runnable getStoreIncompleteRunnable(final RepositorySessionStoreDelegate delegate) {
        return getFlushRecordsRunnabler(delegate);
    }

    private Runnable getFlushRecordsRunnabler(final RepositorySessionStoreDelegate delegate) {
        return new Runnable() {
            @Override
            public void run() {
                synchronized (recordsBufferMonitor) {
                    try {
                        flushNewRecords(delegate);
                    } catch (Exception e) {
                        Logger.warn(LOG_TAG, "Error flushing records to database.", e);
                    }
                }
            }
        };
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
    private void enqueueNewRecord(RepositorySessionStoreDelegate delegate, HistoryRecord record) throws NullCursorException {
        synchronized (recordsBufferMonitor) {
            if (recordsBuffer.size() >= INSERT_RECORD_THRESHOLD) {
                flushNewRecords(delegate);
            }
            Logger.debug(LOG_TAG, "Enqueuing new record with GUID " + record.guid);
            recordsBuffer.add(record);
        }
    }

    private Record addVisitsToRecord(Record record) {
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

    /**
     * Flush queue of incoming records to database.
     * <p>
     * Must be called on <code>storeWorkQueue</code> thread!
     * <p>
     * Must be locked by recordsBufferMonitor!
     * @throws NullCursorException
     */
    private void flushNewRecords(RepositorySessionStoreDelegate delegate) throws NullCursorException {
        if (recordsBuffer.size() < 1) {
            Logger.debug(LOG_TAG, "No records to flush, returning.");
            return;
        }

        final ArrayList<HistoryRecord> outgoing = recordsBuffer;
        recordsBuffer = new ArrayList<HistoryRecord>();
        Logger.debug(LOG_TAG, "Flushing " + outgoing.size() + " records to database.");
        boolean transactionSuccess = ((HistoryDataAccessor) dbHelper).bulkInsert(outgoing);
        if (!transactionSuccess) {
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
            } catch (NoGuidForIdException | ParentNotFoundException e) {
                // Should not happen.
                throw new NullCursorException(e);
            } catch (NullCursorException e) {
                throw e;
            }
            session.trackRecord(succeeded);
            delegate.onRecordStoreSucceeded(succeeded.guid); // At this point, we are really inserted.
        }
    }
}
