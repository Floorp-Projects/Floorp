/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.middleware;

import android.os.SystemClock;
import android.support.annotation.VisibleForTesting;

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

    private ExecutorService storeDelegateExecutor = Executors.newSingleThreadExecutor();

    private volatile boolean storeMarkedIncomplete = false;

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

    @Override
    public void storeIncomplete() {
        storeMarkedIncomplete = true;
    }

    @Override
    public void storeDone() {
        storeDone(System.currentTimeMillis());
    }

    @Override
    public void storeFlush() {
        bufferStorage.flush();
    }

    @Override
    public void storeDone(final long end) {
        doStoreDonePrepare();

        // Determine if we have enough time to perform consistency checks on the buffered data and
        // then store it. If we don't have enough time now, we keep our buffer and try again later.
        // We don't store results of a buffer consistency check anywhere, so we can't treat it
        // separately from storage.
        if (storeMarkedIncomplete || !mayProceedToMergeBuffer()) {
            super.abort();
            storeDelegate.deferredStoreDelegate(storeDelegateExecutor).onStoreCompleted(end);
            return;
        }

        // Separate actual merge, so that it may be tested without involving system clock.
        doStoreDone(end);
    }

    @VisibleForTesting
    public void doStoreDonePrepare() {
        // Now that records stopped flowing, persist them.
        bufferStorage.flush();
    }

    @VisibleForTesting
    public void doStoreDone(final long end) {
        final Collection<Record> buffer = bufferStorage.all();

        // Trivial case of an empty buffer.
        if (buffer.isEmpty()) {
            super.storeDone(end);
            return;
        }

        // Flush our buffer to the wrapped local repository. Data goes live!
        try {
            for (Record record : buffer) {
                this.inner.store(record);
            }
        } catch (NoStoreDelegateException e) {
            // At this point we should have a delegate, so this won't happen.
        }

        // And, we're done!
        super.storeDone(end);
    }

    /**
     * When source fails to provide more records, we need to decide what to do with the buffer.
     * We might fail because of a network partition, or because of a concurrent modification of a
     * collection. Either way we do not clear the buffer in a general case. If a collection has been
     * modified, affected records' last-modified timestamps will be bumped, and we will receive those
     * records during the next sync. If we already have them in our buffer, we replace our now-old
     * copy. Otherwise, they are new records and we just append them.
     *
     * We depend on GUIDs to be a primary key for incoming records.
     *
     * @param e indicates reason of failure.
     */
    @Override
    public void sourceFailed(Exception e) {
        bufferStorage.flush();
        super.sourceFailed(e);
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

    @Override
    public long getHighWaterMarkTimestamp() {
        return bufferStorage.latestModifiedTimestamp();
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
