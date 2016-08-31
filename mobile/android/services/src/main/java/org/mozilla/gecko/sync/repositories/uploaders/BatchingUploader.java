/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.repositories.uploaders;

import android.net.Uri;
import android.support.annotation.VisibleForTesting;

import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.sync.InfoConfiguration;
import org.mozilla.gecko.sync.Server11RecordPostFailedException;
import org.mozilla.gecko.sync.net.SyncResponse;
import org.mozilla.gecko.sync.net.SyncStorageResponse;
import org.mozilla.gecko.sync.repositories.Server11RepositorySession;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionStoreDelegate;
import org.mozilla.gecko.sync.repositories.domain.Record;

import java.util.ArrayList;
import java.util.concurrent.Executor;
import java.util.concurrent.atomic.AtomicLong;

/**
 * Uploader which implements batching introduced in Sync 1.5.
 *
 * Batch vs payload terminology:
 * - batch is comprised of a series of payloads, which are all committed at the same time.
 * -- identified via a "batch token", which is returned after first payload for the batch has been uploaded.
 * - payload is a collection of records which are uploaded together. Associated with a batch.
 * -- last payload, identified via commit=true, commits the batch.
 *
 * Limits for how many records can fit into a payload and into a batch are defined in the passed-in
 * InfoConfiguration object.
 *
 * If we can't fit everything we'd like to upload into one batch (according to max-total-* limits),
 * then we commit that batch, and start a new one. There are no explicit limits on total number of
 * batches we might use, although at some point we'll start to run into storage limit errors from the API.
 *
 * Once we go past using one batch this uploader is no longer "atomic". Partial state is exposed
 * to other clients after our first batch is committed and before our last batch is committed.
 * However, our per-batch limits are high, X-I-U-S mechanics help protect downloading clients
 * (as long as they implement X-I-U-S) with 412 error codes in case of interleaving upload and download,
 * and most mobile clients will not be uploading large-enough amounts of data (especially structured
 * data, such as bookmarks).
 *
 * Last-Modified header returned with the first batch payload POST success is maintained for a batch,
 * to guard against concurrent-modification errors (different uploader commits before we're done).
 *
 * Non-batching mode notes:
 * We also support Sync servers which don't enable batching for uploads. In this case, we respect
 * payload limits for individual uploads, and every upload is considered a commit. Batching limits
 * do not apply, and batch token is irrelevant.
 * We do keep track of Last-Modified and send along X-I-U-S with our uploads, to protect against
 * concurrent modifications by other clients.
 */
public class BatchingUploader {
    private static final String LOG_TAG = "BatchingUploader";

    private final Uri collectionUri;

    private volatile boolean recordUploadFailed = false;

    private final BatchMeta batchMeta;
    private final Payload payload;

    // Accessed by synchronously running threads, OK to not synchronize and just make it volatile.
    private volatile Boolean inBatchingMode;

    // Used to ensure we have thread-safe access to the following:
    // - byte and record counts in both Payload and BatchMeta objects
    // - buffers in the Payload object
    private final Object payloadLock = new Object();

    protected Executor workQueue;
    protected final RepositorySessionStoreDelegate sessionStoreDelegate;
    protected final Server11RepositorySession repositorySession;

    protected AtomicLong uploadTimestamp = new AtomicLong(0);

    protected static final int PER_RECORD_OVERHEAD_BYTE_COUNT = RecordUploadRunnable.RECORD_SEPARATOR.length;
    protected static final int PER_PAYLOAD_OVERHEAD_BYTE_COUNT = RecordUploadRunnable.RECORDS_END.length;

    // Sanity check. RECORD_SEPARATOR and RECORD_START are assumed to be of the same length.
    static {
        if (RecordUploadRunnable.RECORD_SEPARATOR.length != RecordUploadRunnable.RECORDS_START.length) {
            throw new IllegalStateException("Separator and start tokens must be of the same length");
        }
    }

    public BatchingUploader(final Server11RepositorySession repositorySession, final Executor workQueue, final RepositorySessionStoreDelegate sessionStoreDelegate) {
        this.repositorySession = repositorySession;
        this.workQueue = workQueue;
        this.sessionStoreDelegate = sessionStoreDelegate;
        this.collectionUri = Uri.parse(repositorySession.getServerRepository().collectionURI().toString());

        InfoConfiguration config = repositorySession.getServerRepository().getInfoConfiguration();
        this.batchMeta = new BatchMeta(
                payloadLock, config.maxTotalBytes, config.maxTotalRecords,
                repositorySession.getServerRepository().getCollectionLastModified()
        );
        this.payload = new Payload(payloadLock, config.maxPostBytes, config.maxPostRecords);
    }

    public void process(final Record record) {
        final String guid = record.guid;
        final byte[] recordBytes = record.toJSONBytes();
        final long recordDeltaByteCount = recordBytes.length + PER_RECORD_OVERHEAD_BYTE_COUNT;

        Logger.debug(LOG_TAG, "Processing a record with guid: " + guid);

        // We can't upload individual records which exceed our payload byte limit.
        if ((recordDeltaByteCount + PER_PAYLOAD_OVERHEAD_BYTE_COUNT) > payload.maxBytes) {
            sessionStoreDelegate.onRecordStoreFailed(new RecordTooLargeToUpload(), guid);
            return;
        }

        synchronized (payloadLock) {
            final boolean canFitRecordIntoBatch = batchMeta.canFit(recordDeltaByteCount);
            final boolean canFitRecordIntoPayload = payload.canFit(recordDeltaByteCount);

            // Record fits!
            if (canFitRecordIntoBatch && canFitRecordIntoPayload) {
                Logger.debug(LOG_TAG, "Record fits into the current batch and payload");
                addAndFlushIfNecessary(recordDeltaByteCount, recordBytes, guid);

            // Payload won't fit the record.
            } else if (canFitRecordIntoBatch) {
                Logger.debug(LOG_TAG, "Current payload won't fit incoming record, uploading payload.");
                flush(false, false);

                Logger.debug(LOG_TAG, "Recording the incoming record into a new payload");

                // Keep track of the overflow record.
                addAndFlushIfNecessary(recordDeltaByteCount, recordBytes, guid);

            // Batch won't fit the record.
            } else {
                Logger.debug(LOG_TAG, "Current batch won't fit incoming record, committing batch.");
                flush(true, false);

                Logger.debug(LOG_TAG, "Recording the incoming record into a new batch");
                batchMeta.reset();

                // Keep track of the overflow record.
                addAndFlushIfNecessary(recordDeltaByteCount, recordBytes, guid);
            }
        }
    }

    // Convenience function used from the process method; caller must hold a payloadLock.
    private void addAndFlushIfNecessary(long byteCount, byte[] recordBytes, String guid) {
        boolean isPayloadFull = payload.addAndEstimateIfFull(byteCount, recordBytes, guid);
        boolean isBatchFull = batchMeta.addAndEstimateIfFull(byteCount);

        // Preemptive commit batch or upload a payload if they're estimated to be full.
        if (isBatchFull) {
            flush(true, false);
            batchMeta.reset();
        } else if (isPayloadFull) {
            flush(false, false);
        }
    }

    public void noMoreRecordsToUpload() {
        Logger.debug(LOG_TAG, "Received 'no more records to upload' signal.");

        // Run this after the last payload succeeds, so that we know for sure if we're in a batching
        // mode and need to commit with a potentially empty payload.
        workQueue.execute(new Runnable() {
            @Override
            public void run() {
                commitIfNecessaryAfterLastPayload();
            }
        });
    }

    @VisibleForTesting
    protected void commitIfNecessaryAfterLastPayload() {
        // Must be called after last payload upload finishes.
        synchronized (payload) {
            // If we have any pending records in the Payload, flush them!
            if (!payload.isEmpty()) {
                flush(true, true);

            // If we have an empty payload but need to commit the batch in the batching mode, flush!
            } else if (batchMeta.needToCommit() && Boolean.TRUE.equals(inBatchingMode)) {
                flush(true, true);

            // Otherwise, we're done.
            } else {
                finished(uploadTimestamp);
            }
        }
    }

    /**
     * We've been told by our upload delegate that a payload succeeded.
     * Depending on the type of payload and batch mode status, inform our delegate of progress.
     *
     * @param response success response to our commit post
     * @param isCommit was this a commit upload?
     * @param isLastPayload was this a very last payload we'll upload?
     */
    public void payloadSucceeded(final SyncStorageResponse response, final boolean isCommit, final boolean isLastPayload) {
        // Sanity check.
        if (inBatchingMode == null) {
            throw new IllegalStateException("Can't process payload success until we know if we're in a batching mode");
        }

        // We consider records to have been committed if we're not in a batching mode or this was a commit.
        // If records have been committed, notify our store delegate.
        if (!inBatchingMode || isCommit) {
            for (String guid : batchMeta.getSuccessRecordGuids()) {
                sessionStoreDelegate.onRecordStoreSucceeded(guid);
            }
        }

        // If this was our very last commit, we're done storing records.
        // Get Last-Modified timestamp from the response, and pass it upstream.
        if (isLastPayload) {
            finished(response.normalizedTimestampForHeader(SyncResponse.X_LAST_MODIFIED));
        }
    }

    public void lastPayloadFailed() {
        finished(uploadTimestamp);
    }

    private void finished(long lastModifiedTimestamp) {
        bumpTimestampTo(uploadTimestamp, lastModifiedTimestamp);
        finished(uploadTimestamp);
    }

    private void finished(AtomicLong lastModifiedTimestamp) {
        repositorySession.storeDone(lastModifiedTimestamp.get());
    }

    public BatchMeta getCurrentBatch() {
        return batchMeta;
    }

    public void setInBatchingMode(boolean inBatchingMode) {
        this.inBatchingMode = inBatchingMode;

        // If we know for sure that we're not in a batching mode,
        // consider our batch to be of unlimited size.
        this.batchMeta.setIsUnlimited(!inBatchingMode);
    }

    public Boolean getInBatchingMode() {
        return inBatchingMode;
    }

    public void setLastModified(final Long lastModified, final boolean isCommit) throws BatchingUploaderException {
        // Sanity check.
        if (inBatchingMode == null) {
            throw new IllegalStateException("Can't process Last-Modified before we know we're in a batching mode.");
        }

        // In non-batching mode, every time we receive a Last-Modified timestamp, we expect it to change
        // since records are "committed" (become visible to other clients) on every payload.
        // In batching mode, we only expect Last-Modified to change when we commit a batch.
        batchMeta.setLastModified(lastModified, isCommit || !inBatchingMode);
    }

    public void recordSucceeded(final String recordGuid) {
        Logger.debug(LOG_TAG, "Record store succeeded: " + recordGuid);
        batchMeta.recordSucceeded(recordGuid);
    }

    public void recordFailed(final String recordGuid) {
        recordFailed(new Server11RecordPostFailedException(), recordGuid);
    }

    public void recordFailed(final Exception e, final String recordGuid) {
        Logger.debug(LOG_TAG, "Record store failed for guid " + recordGuid + " with exception: " + e.toString());
        recordUploadFailed = true;
        sessionStoreDelegate.onRecordStoreFailed(e, recordGuid);
    }

    public Server11RepositorySession getRepositorySession() {
        return repositorySession;
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

    private void flush(final boolean isCommit, final boolean isLastPayload) {
        final ArrayList<byte[]> outgoing;
        final ArrayList<String> outgoingGuids;
        final long byteCount;

        // Even though payload object itself is thread-safe, we want to ensure we get these altogether
        // as a "unit". Another approach would be to create a wrapper object for these values, but this works.
        synchronized (payloadLock) {
            outgoing = payload.getRecordsBuffer();
            outgoingGuids = payload.getRecordGuidsBuffer();
            byteCount = payload.getByteCount();
        }

        workQueue.execute(new RecordUploadRunnable(
                new BatchingAtomicUploaderMayUploadProvider(),
                collectionUri,
                batchMeta,
                new PayloadUploadDelegate(this, outgoingGuids, isCommit, isLastPayload),
                outgoing,
                byteCount,
                isCommit
        ));

        payload.reset();
    }

    private class BatchingAtomicUploaderMayUploadProvider implements MayUploadProvider {
        public boolean mayUpload() {
            return !recordUploadFailed;
        }
    }

    public static class BatchingUploaderException extends Exception {
        private static final long serialVersionUID = 1L;
    }
    public static class RecordTooLargeToUpload extends BatchingUploaderException {
        private static final long serialVersionUID = 1L;
    }
    public static class LastModifiedDidNotChange extends BatchingUploaderException {
        private static final long serialVersionUID = 1L;
    }
    public static class LastModifiedChangedUnexpectedly extends BatchingUploaderException {
        private static final long serialVersionUID = 1L;
    }
    public static class TokenModifiedException extends BatchingUploaderException {
        private static final long serialVersionUID = 1L;
    };
}