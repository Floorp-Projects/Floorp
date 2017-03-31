/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.middleware;

import android.os.SystemClock;
import android.support.annotation.VisibleForTesting;

import org.mozilla.gecko.sync.SyncDeadlineReachedException;
import org.mozilla.gecko.sync.middleware.storage.BufferStorage;
import org.mozilla.gecko.sync.repositories.InactiveSessionException;
import org.mozilla.gecko.sync.repositories.NoStoreDelegateException;
import org.mozilla.gecko.sync.repositories.RepositorySession;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionFetchRecordsDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionStoreDelegate;
import org.mozilla.gecko.sync.repositories.domain.Record;

import java.util.Collection;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

/**
 * Buffering middleware which is intended to wrap local RepositorySessions.
 *
 * Configure it:
 *  - with an appropriate BufferStore (in-memory, record-type-aware database-backed, etc).
 *
 *  Fetch is pass-through, store is buffered.
 *
 * @author grisha
 */
/* package-local */ class BufferingMiddlewareRepositorySession extends MiddlewareRepositorySession {
    private final BufferStorage bufferStorage;
    private final long syncDeadlineMillis;

    /* package-local */ BufferingMiddlewareRepositorySession(
            RepositorySession repositorySession, MiddlewareRepository repository,
            long syncDeadlineMillis, BufferStorage bufferStorage) {
        super(repositorySession, repository);
        this.syncDeadlineMillis = syncDeadlineMillis;
        this.bufferStorage = bufferStorage;
    }

    @Override
    public void fetchSince(long timestamp, RepositorySessionFetchRecordsDelegate delegate) {
        this.inner.fetchSince(timestamp, delegate);
    }

    @Override
    public void fetch(String[] guids, RepositorySessionFetchRecordsDelegate delegate) throws InactiveSessionException {
        this.inner.fetch(guids, delegate);
    }

    @Override
    public void fetchAll(RepositorySessionFetchRecordsDelegate delegate) {
        this.inner.fetchAll(delegate);
    }

    /**
     * Will be called when this repository is acting as a `source`, and a flow of records into `sink`
     * was completed. That is, we've uploaded merged records to the server, so now is a good time
     * to clean up our buffer for this repository.
     */
    @Override
    public void performCleanup() {
        bufferStorage.clear();
    }

    @Override
    public void store(Record record) throws NoStoreDelegateException {
        bufferStorage.addOrReplace(record);
    }

    /**
     * When source fails to provide all records, we need to decide what to do with the buffer.
     * We might fail because of a network partition, or because of a concurrent modification of a
     * collection, or because we ran out of time fetching records, or some other reason.
     *
     * Either way we do not clear the buffer in any error scenario, but rather
     * allow it to be re-filled, replacing existing records with their newer versions if necessary.
     *
     * If a collection has been modified, affected records' last-modified timestamps will be bumped,
     * and we will receive those records during the next sync. If we already have them in our buffer,
     * we replace our now-old copy. Otherwise, they are new records and we just append them.
     *
     * Incoming records are mapped to existing ones via GUIDs.
     */
    @Override
    public void storeIncomplete() {
        bufferStorage.flush();
    }

    @Override
    public void storeFlush() {
        bufferStorage.flush();
    }

    @Override
    public void storeDone() {
        bufferStorage.flush();

        // Determine if we have enough time to merge the buffer data.
        // If we don't have enough time now, we keep our buffer and try again later.
        if (!mayProceedToMergeBuffer()) {
            super.abort();
            storeDelegate.deferredStoreDelegate(storeWorkQueue).onStoreFailed(new SyncDeadlineReachedException());
            return;
        }

        doMergeBuffer();
    }

    @VisibleForTesting
    /* package-local */ void doMergeBuffer() {
        final Collection<Record> bufferData = bufferStorage.all();

        // Trivial case of an empty buffer.
        if (bufferData.isEmpty()) {
            super.storeDone();
            return;
        }

        // Let session handle actual storing of records as it pleases.
        // See Bug 1332094 which is concerned with allowing merge to proceed transactionally.
        try {
            for (Record record : bufferData) {
                this.inner.store(record);
            }
        } catch (NoStoreDelegateException e) {
            // At this point we should have a store delegate set on the session, so this won't happen.
        }

        // Let session know that there are no more records to store.
        super.storeDone();
    }

    /**
     * Session abnormally aborted. This doesn't mean our so-far buffered data is invalid.
     * Clean up after ourselves, if there's anything to clean up.
     */
    @Override
    public void abort() {
        bufferStorage.flush();
        super.abort();
    }

    @Override
    public void setStoreDelegate(RepositorySessionStoreDelegate delegate) {
        inner.setStoreDelegate(delegate);
        this.storeDelegate = delegate;
    }

    private boolean mayProceedToMergeBuffer() {
        // If our buffer storage is not persistent, disallowing merging after buffer has been filled
        // means throwing away records only to re-download them later.
        // In this case allow merge to proceed even if we're past the deadline.
        if (!bufferStorage.isPersistent()) {
            return true;
        }

        // While actual runtime of a merge operation is a function of record type, buffer size, etc.,
        // let's do a simple thing for now and say that we may proceed if we have couple of minutes
        // of runtime left. That surely is enough, right?
        final long timeLeftMillis = syncDeadlineMillis - SystemClock.elapsedRealtime();
        return timeLeftMillis > 1000 * 60 * 2;
    }
}
