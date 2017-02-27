/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.repositories.uploaders;

import android.support.annotation.Nullable;
import android.support.annotation.VisibleForTesting;

import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.sync.CollectionConcurrentModificationException;
import org.mozilla.gecko.sync.Server15RecordPostFailedException;
import org.mozilla.gecko.sync.net.SyncResponse;
import org.mozilla.gecko.sync.net.SyncStorageResponse;

import java.util.ArrayList;
import java.util.concurrent.Executor;
import java.util.concurrent.atomic.AtomicLong;

/**
 * Dispatches payload runnables and handles their results.
 *
 * All of the methods, except for `queue` and `finalizeQueue`, will be called from the thread(s)
 * running sequentially on the SingleThreadExecutor `executor`.
 */
class PayloadDispatcher {
    private static final String LOG_TAG = "PayloadDispatcher";

    // All payload runnables share the same whiteboard.
    // It's accessed directly by the runnables; tests also make use of this direct access.
    volatile BatchMeta batchWhiteboard;
    private final AtomicLong uploadTimestamp = new AtomicLong(0);

    private final Executor executor;
    private final BatchingUploader uploader;

    // For both of these flags:
    // Written from sequentially running thread(s) on the SingleThreadExecutor `executor`.
    // Read by many threads running concurrently on the records consumer thread pool.
    volatile boolean recordUploadFailed = false;
    volatile boolean storeFailed = false;

    PayloadDispatcher(Executor executor, BatchingUploader uploader, @Nullable Long initialLastModified) {
        // Initially we don't know if we're in a batching mode.
        this.batchWhiteboard = new BatchMeta(initialLastModified, null);
        this.uploader = uploader;
        this.executor = executor;
    }

    void queue(
            final ArrayList<byte[]> outgoing,
            final ArrayList<String> outgoingGuids,
            final long byteCount,
            final boolean isCommit, final boolean isLastPayload) {

        // Note that `executor` is expected to be a SingleThreadExecutor.
        executor.execute(new BatchContextRunnable(isCommit) {
            @Override
            public void run() {
                createRecordUploadRunnable(outgoing, outgoingGuids, byteCount, isCommit, isLastPayload).run();
            }
        });
    }

    void setInBatchingMode(boolean inBatchingMode) {
        batchWhiteboard.setInBatchingMode(inBatchingMode);
        uploader.setUnlimitedMode(!inBatchingMode);
    }

    /**
     * We've been told by our upload delegate that a payload succeeded.
     * Depending on the type of payload and batch mode status, inform our delegate of progress.
     *
     * @param response success response to our commit post
     * @param guids list of successfully posted record guids
     * @param isCommit was this a commit upload?
     * @param isLastPayload was this a very last payload we'll upload?
     */
    void payloadSucceeded(final SyncStorageResponse response, final String[] guids, final boolean isCommit, final boolean isLastPayload) {
        // Sanity check.
        if (batchWhiteboard.getInBatchingMode() == null) {
            throw new IllegalStateException("Can't process payload success until we know if we're in a batching mode");
        }

        // We consider records to have been committed if we're not in a batching mode or this was a commit.
        // If records have been committed, notify our store delegate.
        if (!batchWhiteboard.getInBatchingMode() || isCommit) {
            for (String guid : guids) {
                uploader.sessionStoreDelegate.onRecordStoreSucceeded(guid);
            }
        }

        // If this was our very last commit, we're done storing records.
        // Get Last-Modified timestamp from the response, and pass it upstream.
        if (isLastPayload) {
            finished(response.normalizedTimestampForHeader(SyncResponse.X_LAST_MODIFIED));
        }
    }

    void lastPayloadFailed() {
        uploader.finished(uploadTimestamp);
    }

    private void finished(long lastModifiedTimestamp) {
        bumpTimestampTo(uploadTimestamp, lastModifiedTimestamp);
        uploader.finished(uploadTimestamp);
    }

    void finalizeQueue(final boolean needToCommit, final Runnable finalRunnable) {
        executor.execute(new NonPayloadContextRunnable() {
            @Override
            public void run() {
                // Must be called after last payload upload finishes.
                if (needToCommit && Boolean.TRUE.equals(batchWhiteboard.getInBatchingMode())) {
                    finalRunnable.run();

                    // Otherwise, we're done.
                } else {
                    uploader.finished(uploadTimestamp);
                }
            }
        });
    }

    void recordFailed(final String recordGuid) {
        recordFailed(new Server15RecordPostFailedException(), recordGuid);
    }

    void recordFailed(final Exception e, final String recordGuid) {
        Logger.debug(LOG_TAG, "Record store failed for guid " + recordGuid + " with exception: " + e.toString());
        recordUploadFailed = true;
        uploader.sessionStoreDelegate.onRecordStoreFailed(e, recordGuid);
    }

    void concurrentModificationDetected() {
        recordUploadFailed = true;
        storeFailed = true;
        uploader.sessionStoreDelegate.onStoreFailed(new CollectionConcurrentModificationException());
    }

    void prepareForNextBatch() {
        batchWhiteboard = batchWhiteboard.nextBatchMeta();
    }

    private static void bumpTimestampTo(final AtomicLong current, long newValue) {
        while (true) {
            long existing = current.get();
            if (existing > newValue) {
                return;
            }
            if (current.compareAndSet(existing, newValue)) {
                return;
            }
        }
    }

    /**
     * Allows tests to define their own RecordUploadRunnable.
     */
    @VisibleForTesting
    Runnable createRecordUploadRunnable(final ArrayList<byte[]> outgoing,
                                                  final ArrayList<String> outgoingGuids,
                                                  final long byteCount,
                                                  final boolean isCommit, final boolean isLastPayload) {
        return new RecordUploadRunnable(
                new BatchingAtomicUploaderMayUploadProvider(),
                uploader.collectionUri,
                batchWhiteboard.getToken(),
                new PayloadUploadDelegate(
                        uploader.authHeaderProvider,
                        PayloadDispatcher.this,
                        outgoingGuids,
                        isCommit,
                        isLastPayload
                ),
                outgoing,
                byteCount,
                isCommit
        );
    }

    /**
     * Allows tests to easily peek into the flow of upload tasks.
     */
    @VisibleForTesting
    abstract static class BatchContextRunnable implements Runnable {
        boolean isCommit;

        BatchContextRunnable(boolean isCommit) {
            this.isCommit = isCommit;
        }
    }

    /**
     * Allows tests to tell apart non-payload runnables going through the executor.
     */
    @VisibleForTesting
    abstract static class NonPayloadContextRunnable implements Runnable {}

    // Instances of this class must be accessed from threads running on the `executor`.
    private class BatchingAtomicUploaderMayUploadProvider implements MayUploadProvider {
        public boolean mayUpload() {
            return !recordUploadFailed;
        }
    }
}
