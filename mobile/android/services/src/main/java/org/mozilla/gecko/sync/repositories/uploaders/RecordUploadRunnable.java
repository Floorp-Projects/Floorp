/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.repositories.uploaders;

import android.net.Uri;
import android.support.annotation.Nullable;
import android.support.annotation.VisibleForTesting;

import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.sync.Utils;
import org.mozilla.gecko.sync.Server15PreviousPostFailedException;
import org.mozilla.gecko.sync.net.SyncStorageRequest;
import org.mozilla.gecko.sync.net.SyncStorageRequestDelegate;

import java.io.IOException;
import java.io.OutputStream;
import java.net.URI;
import java.net.URISyntaxException;
import java.util.ArrayList;

import ch.boye.httpclientandroidlib.entity.ContentProducer;
import ch.boye.httpclientandroidlib.entity.EntityTemplate;

/**
 * Responsible for creating and posting a <code>SyncStorageRequest</code> request object.
 */
public class RecordUploadRunnable implements Runnable {
    public final String LOG_TAG = "RecordUploadRunnable";

    public final static byte[] RECORDS_START = { 91 };      // [ in UTF-8
    public final static byte[] RECORD_SEPARATOR = { 44 };   // , in UTF-8
    public final static byte[] RECORDS_END = { 93 };        // ] in UTF-8

    private static final String QUERY_PARAM_BATCH = "batch";
    private static final String QUERY_PARAM_TRUE = "true";
    private static final String QUERY_PARAM_BATCH_COMMIT = "commit";

    private final MayUploadProvider mayUploadProvider;
    private final SyncStorageRequestDelegate uploadDelegate;

    private final ArrayList<byte[]> outgoing;
    private final long byteCount;

    // Used to construct POST URI during run().
    @VisibleForTesting
    public final boolean isCommit;
    private final Uri collectionUri;
    private final String batchToken;

    public RecordUploadRunnable(MayUploadProvider mayUploadProvider,
                                Uri collectionUri,
                                String batchToken,
                                SyncStorageRequestDelegate uploadDelegate,
                                ArrayList<byte[]> outgoing,
                                long byteCount,
                                boolean isCommit) {
        this.mayUploadProvider = mayUploadProvider;
        this.uploadDelegate = uploadDelegate;
        this.outgoing = outgoing;
        this.byteCount = byteCount;
        this.batchToken = batchToken;
        this.collectionUri = collectionUri;
        this.isCommit = isCommit;
    }

    public static class ByteArraysContentProducer implements ContentProducer {
        ArrayList<byte[]> outgoing;
        public ByteArraysContentProducer(ArrayList<byte[]> arrays) {
            outgoing = arrays;
        }

        @Override
        public void writeTo(OutputStream outstream) throws IOException {
            int count = outgoing.size();
            outstream.write(RECORDS_START);
            if (count > 0) {
                outstream.write(outgoing.get(0));
                for (int i = 1; i < count; ++i) {
                    outstream.write(RECORD_SEPARATOR);
                    outstream.write(outgoing.get(i));
                }
            }
            outstream.write(RECORDS_END);
        }

        public static long outgoingBytesCount(ArrayList<byte[]> outgoing) {
            final long numberOfRecords = outgoing.size();

            // Account for start and end tokens.
            long count = RECORDS_START.length + RECORDS_END.length;

            // Account for all the records.
            for (int i = 0; i < numberOfRecords; i++) {
                count += outgoing.get(i).length;
            }

            // Account for a separator between the records.
            // There's one less separator than there are records.
            if (numberOfRecords > 1) {
                count += RECORD_SEPARATOR.length * (numberOfRecords - 1);
            }

            return count;
        }
    }

    public static class ByteArraysEntity extends EntityTemplate {
        private final long count;
        public ByteArraysEntity(ArrayList<byte[]> arrays, long totalBytes) {
            super(new ByteArraysContentProducer(arrays));
            this.count = totalBytes;
            this.setContentType("application/json");
            // charset is set in BaseResource.

            // Sanity check our byte counts.
            long realByteCount = ByteArraysContentProducer.outgoingBytesCount(arrays);
            if (realByteCount != totalBytes) {
                throw new IllegalStateException("Mismatched byte counts. Received " + totalBytes + " while real byte count is " + realByteCount);
            }
        }

        @Override
        public long getContentLength() {
            return count;
        }

        @Override
        public boolean isRepeatable() {
            return true;
        }
    }

    @Override
    public void run() {
        if (!mayUploadProvider.mayUpload()) {
            Logger.info(LOG_TAG, "Told not to proceed by the uploader. Cancelling upload, failing records.");
            uploadDelegate.handleRequestError(new Server15PreviousPostFailedException());
            return;
        }

        Logger.trace(LOG_TAG, "Running upload task. Outgoing records: " + outgoing.size());

        // We don't want the task queue to proceed until this request completes.
        // Fortunately, BaseResource is currently synchronous.
        // If that ever changes, you'll need to block here.

        final URI postURI = buildPostURI(isCommit, batchToken, collectionUri);
        final SyncStorageRequest request = new SyncStorageRequest(postURI);
        request.delegate = uploadDelegate;

        ByteArraysEntity body = new ByteArraysEntity(outgoing, byteCount);
        request.post(body);
    }

    @VisibleForTesting
    public static URI buildPostURI(boolean isCommit, @Nullable String batchToken, Uri collectionUri) {
        final Uri.Builder uriBuilder = collectionUri.buildUpon();

        if (batchToken != null) {
            uriBuilder.appendQueryParameter(QUERY_PARAM_BATCH, batchToken);
        } else {
            uriBuilder.appendQueryParameter(QUERY_PARAM_BATCH, QUERY_PARAM_TRUE);
        }

        if (isCommit) {
            uriBuilder.appendQueryParameter(QUERY_PARAM_BATCH_COMMIT, QUERY_PARAM_TRUE);
        }

        try {
            return new URI(uriBuilder.build().toString());
        } catch (URISyntaxException e) {
            throw new IllegalStateException("Failed to construct a collection URI", e);
        }
    }
}
