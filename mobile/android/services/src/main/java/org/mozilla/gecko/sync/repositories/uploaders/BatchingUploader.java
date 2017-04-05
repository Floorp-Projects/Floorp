/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.repositories.uploaders;

import android.net.Uri;
import android.support.annotation.VisibleForTesting;

import org.json.simple.JSONObject;
import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.sync.CryptoRecord;
import org.mozilla.gecko.sync.InfoConfiguration;
import org.mozilla.gecko.sync.Server15PreviousPostFailedException;
import org.mozilla.gecko.sync.net.AuthHeaderProvider;
import org.mozilla.gecko.sync.repositories.RepositorySession;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionStoreDelegate;
import org.mozilla.gecko.sync.repositories.domain.Record;

import java.util.ArrayList;
import java.util.concurrent.ExecutorService;
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
 * However, our per-batch limits are (hopefully) high, X-I-U-S mechanics help protect downloading clients
 * (as long as they implement X-I-U-S) with 412 error codes in case of interleaving upload and download.
 *
 * Last-Modified header returned with the first batch payload POST success is maintained for a batch,
 * to guard against concurrent-modification errors (different uploader commits before we're done).
 *
 * Implementation notes:
 * - RecordsChannel (via RepositorySession) delivers a stream of records for upload via {@link #process(Record)}
 * - UploaderMeta is used to track batch-level information necessary for processing outgoing records
 * - PayloadMeta is used to track payload-level information necessary for processing outgoing records
 * - BatchMeta within PayloadDispatcher acts as a shared whiteboard which is used for tracking
 *   information across batches (last-modified, batching mode) as well as batch side-effects (stored guids)
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

    private static final int PER_RECORD_OVERHEAD_BYTE_COUNT = RecordUploadRunnable.RECORD_SEPARATOR.length;
    /* package-local */ static final int PER_PAYLOAD_OVERHEAD_BYTE_COUNT = RecordUploadRunnable.RECORDS_END.length;

    // Sanity check. RECORD_SEPARATOR and RECORD_START are assumed to be of the same length.
    static {
        if (RecordUploadRunnable.RECORD_SEPARATOR.length != RecordUploadRunnable.RECORDS_START.length) {
            throw new IllegalStateException("Separator and start tokens must be of the same length");
        }
    }

    // Accessed by the record consumer thread pool.
    private final ExecutorService executor;
    // Will be re-created, so mark it as volatile.
    private volatile Payload payload;

    // Accessed by both the record consumer thread pool and the network worker thread(s).
    /* package-local */ final Uri collectionUri;
    /* package-local */ final RepositorySessionStoreDelegate sessionStoreDelegate;
    /* package-local */ @VisibleForTesting final PayloadDispatcher payloadDispatcher;
    /* package-local */ final AuthHeaderProvider authHeaderProvider;
    private final RepositorySession repositorySession;
    // Will be re-created, so mark it as volatile.
    private volatile UploaderMeta uploaderMeta;

    // Used to ensure we have thread-safe access to the following:
    // - byte and record counts in both Payload and BatchMeta objects
    // - buffers in the Payload object
    private final Object payloadLock = new Object();

    // Maximum size of a BSO's payload field that server will accept during an upload.
    // Our naming here is somewhat unfortunate, since we're calling two different things a "payload".
    // In context of the uploader, a "payload" is an entirety of what gets POST-ed to the server in
    // an individual request.
    // In context of Sync Storage, "payload" is a BSO (basic storage object) field defined as: "a
    // string containing the data of the record."
    // Sync Storage servers place a hard limit on how large a payload _field_ might be, and so we
    // maintain this limit for a single sanity check.
    private final long maxPayloadFieldBytes;

    public BatchingUploader(
            final RepositorySession repositorySession, final ExecutorService workQueue,
            final RepositorySessionStoreDelegate sessionStoreDelegate, final Uri baseCollectionUri,
            final Long localCollectionLastModified, final InfoConfiguration infoConfiguration,
            final AuthHeaderProvider authHeaderProvider) {
        this.repositorySession = repositorySession;
        this.sessionStoreDelegate = sessionStoreDelegate;
        this.collectionUri = baseCollectionUri;
        this.authHeaderProvider = authHeaderProvider;

        this.uploaderMeta = new UploaderMeta(
                payloadLock, infoConfiguration.maxTotalBytes, infoConfiguration.maxTotalRecords);
        this.payload = new Payload(
                payloadLock, infoConfiguration.maxPostBytes, infoConfiguration.maxPostRecords);

        this.payloadDispatcher = createPayloadDispatcher(workQueue, localCollectionLastModified);

        this.maxPayloadFieldBytes = infoConfiguration.maxPayloadBytes;

        this.executor = workQueue;
    }

    // Called concurrently from the threads running off of a record consumer thread pool.
    public void process(final Record record) {
        final String guid = record.guid;

        // If store failed entirely, just bail out. We've already told our delegate that we failed.
        if (payloadDispatcher.storeFailed) {
            return;
        }

        // If a record or a payload failed, we won't let subsequent requests proceed.
        // This means that we may bail much earlier.
        if (payloadDispatcher.recordUploadFailed) {
            sessionStoreDelegate.deferredStoreDelegate(executor).onRecordStoreFailed(
                    new Server15PreviousPostFailedException(), guid
            );
            return;
        }

        final JSONObject recordJSON = record.toJSONObject();

        final String payloadField = (String) recordJSON.get(CryptoRecord.KEY_PAYLOAD);
        if (payloadField == null) {
            sessionStoreDelegate.deferredStoreDelegate(executor).onRecordStoreFailed(
                    new IllegalRecordException(), guid
            );
            return;
        }

        // We can't upload individual records whose payload fields exceed our payload field byte limit.
        // UTF-8 uses 1 byte per character for the ASCII range. Contents of the payloadField are
        // base64 and hex encoded, so character count is sufficient.
        if (payloadField.length() > this.maxPayloadFieldBytes) {
            sessionStoreDelegate.deferredStoreDelegate(executor).onRecordStoreFailed(
                    new PayloadTooLargeToUpload(), guid
            );
            return;
        }

        final byte[] recordBytes = Record.stringToJSONBytes(recordJSON.toJSONString());
        if (recordBytes == null) {
            sessionStoreDelegate.deferredStoreDelegate(executor).onRecordStoreFailed(
                    new IllegalRecordException(), guid
            );
            return;
        }

        final long recordDeltaByteCount = recordBytes.length + PER_RECORD_OVERHEAD_BYTE_COUNT;
        Logger.debug(LOG_TAG, "Processing a record with guid: " + guid);

        // We can't upload individual records which exceed our payload total byte limit.
        if ((recordDeltaByteCount + PER_PAYLOAD_OVERHEAD_BYTE_COUNT) > payload.maxBytes) {
            sessionStoreDelegate.deferredStoreDelegate(executor).onRecordStoreFailed(
                    new RecordTooLargeToUpload(), guid
            );
            return;
        }

        synchronized (payloadLock) {
            final boolean canFitRecordIntoBatch = uploaderMeta.canFit(recordDeltaByteCount);
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

                // Keep track of the overflow record.
                addAndFlushIfNecessary(recordDeltaByteCount, recordBytes, guid);
            }
        }
    }

    // Convenience function used from the process method; caller must hold a payloadLock.
    private void addAndFlushIfNecessary(long byteCount, byte[] recordBytes, String guid) {
        boolean isPayloadFull = payload.addAndEstimateIfFull(byteCount, recordBytes, guid);
        boolean isBatchFull = uploaderMeta.addAndEstimateIfFull(byteCount);

        // Preemptive commit batch or upload a payload if they're estimated to be full.
        if (isBatchFull) {
            flush(true, false);
        } else if (isPayloadFull) {
            flush(false, false);
        }
    }

    public void noMoreRecordsToUpload() {
        Logger.debug(LOG_TAG, "Received 'no more records to upload' signal.");

        // If we have any pending records in the Payload, flush them!
        if (!payload.isEmpty()) {
            flush(true, true);
            return;
        }

        // If we don't have any pending records, we still might need to send an empty "commit"
        // payload if we are in the batching mode.
        // The dispatcher will run the final flush on its executor if necessary after all payloads
        // succeed and it knows for sure if we're in a batching mode.
        payloadDispatcher.finalizeQueue(uploaderMeta.needToCommit(), new Runnable() {
            @Override
            public void run() {
                flush(true, true);
            }
        });
    }

    /* package-local */ void finished(AtomicLong lastModifiedTimestamp) {
        sessionStoreDelegate.deferredStoreDelegate(executor).onStoreCompleted(lastModifiedTimestamp.get());
    }

    // Will be called from a thread dispatched by PayloadDispatcher.
    // NB: Access to `uploaderMeta.isUnlimited` is guarded by the payloadLock.
    /* package-local */ void setUnlimitedMode(boolean isUnlimited) {
        // If we know for sure that we're not in a batching mode,
        // consider our batch to be of unlimited size.
        this.uploaderMeta.setIsUnlimited(isUnlimited);
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
        payload = payload.nextPayload();

        payloadDispatcher.queue(outgoing, outgoingGuids, byteCount, isCommit, isLastPayload);

        if (isCommit && !isLastPayload) {
            uploaderMeta = uploaderMeta.nextUploaderMeta();
        }
    }

    /**
     * Allows tests to define their own PayloadDispatcher.
     */
    @VisibleForTesting
    PayloadDispatcher createPayloadDispatcher(ExecutorService workQueue, Long localCollectionLastModified) {
        return new PayloadDispatcher(workQueue, this, localCollectionLastModified);
    }

    /* package-local */ static class BatchingUploaderException extends Exception {
        private static final long serialVersionUID = 1L;
    }
    /* package-local */ static class LastModifiedDidNotChange extends BatchingUploaderException {
        private static final long serialVersionUID = 1L;
    }
    /* package-local */ static class LastModifiedChangedUnexpectedly extends BatchingUploaderException {
        private static final long serialVersionUID = 1L;
    }
    /* package-local */ static class TokenModifiedException extends BatchingUploaderException {
        private static final long serialVersionUID = 1L;
    }
    // We may choose what to do about these failures upstream on the delegate level. For now, if a
    // record is failed by the uploader, current sync stage will be aborted - although, uploader
    // doesn't depend on this behaviour.
    private static class RecordTooLargeToUpload extends BatchingUploaderException {
        private static final long serialVersionUID = 1L;
    }
    @VisibleForTesting
    /* package-local */ static class PayloadTooLargeToUpload extends BatchingUploaderException {
        private static final long serialVersionUID = 1L;
    }
    private static class IllegalRecordException extends BatchingUploaderException {
        private static final long serialVersionUID = 1L;
    }
}