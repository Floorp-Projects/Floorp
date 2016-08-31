/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.repositories.uploaders;

import org.json.simple.JSONArray;
import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.sync.ExtendedJSONObject;
import org.mozilla.gecko.sync.HTTPFailureException;
import org.mozilla.gecko.sync.NonArrayJSONException;
import org.mozilla.gecko.sync.NonObjectJSONException;
import org.mozilla.gecko.sync.Utils;
import org.mozilla.gecko.sync.net.AuthHeaderProvider;
import org.mozilla.gecko.sync.net.SyncResponse;
import org.mozilla.gecko.sync.net.SyncStorageRequestDelegate;
import org.mozilla.gecko.sync.net.SyncStorageResponse;

import java.util.ArrayList;

public class PayloadUploadDelegate implements SyncStorageRequestDelegate {
    private static final String LOG_TAG = "PayloadUploadDelegate";

    private static final String KEY_BATCH = "batch";

    private final BatchingUploader uploader;
    private ArrayList<String> postedRecordGuids;
    private final boolean isCommit;
    private final boolean isLastPayload;

    public PayloadUploadDelegate(BatchingUploader uploader, ArrayList<String> postedRecordGuids, boolean isCommit, boolean isLastPayload) {
        this.uploader = uploader;
        this.postedRecordGuids = postedRecordGuids;
        this.isCommit = isCommit;
        this.isLastPayload = isLastPayload;
    }

    @Override
    public AuthHeaderProvider getAuthHeaderProvider() {
        return uploader.getRepositorySession().getServerRepository().getAuthHeaderProvider();
    }

    @Override
    public String ifUnmodifiedSince() {
        final Long lastModified = uploader.getCurrentBatch().getLastModified();
        if (lastModified == null) {
            return null;
        }
        return Utils.millisecondsToDecimalSecondsString(lastModified);
    }

    @Override
    public void handleRequestSuccess(final SyncStorageResponse response) {
        // First, do some sanity checking.
        if (response.getStatusCode() != 200 && response.getStatusCode() != 202) {
            handleRequestError(
                new IllegalStateException("handleRequestSuccess received a non-200/202 response: " + response.getStatusCode())
            );
            return;
        }

        // We always expect to see a Last-Modified header. It's returned with every success response.
        if (!response.httpResponse().containsHeader(SyncResponse.X_LAST_MODIFIED)) {
            handleRequestError(
                    new IllegalStateException("Response did not have a Last-Modified header")
            );
            return;
        }

        // We expect to be able to parse the response as a JSON object.
        final ExtendedJSONObject body;
        try {
            body = response.jsonObjectBody(); // jsonObjectBody() throws or returns non-null.
        } catch (Exception e) {
            Logger.error(LOG_TAG, "Got exception parsing POST success body.", e);
            this.handleRequestError(e);
            return;
        }

        // If we got a 200, it could be either a non-batching result, or a batch commit.
        // - if we're in a batching mode, we expect this to be a commit.
        // If we got a 202, we expect there to be a token present in the response
        if (response.getStatusCode() == 200 && uploader.getCurrentBatch().getToken() != null) {
            if (uploader.getInBatchingMode() && !isCommit) {
                handleRequestError(
                        new IllegalStateException("Got 200 OK in batching mode, but this was not a commit payload")
                );
                return;
            }
        } else if (response.getStatusCode() == 202) {
            if (!body.containsKey(KEY_BATCH)) {
                handleRequestError(
                        new IllegalStateException("Batch response did not have a batch ID")
                );
                return;
            }
        }

        // With sanity checks out of the way, can now safely say if we're in a batching mode or not.
        // We only do this once per session.
        if (uploader.getInBatchingMode() == null) {
            uploader.setInBatchingMode(body.containsKey(KEY_BATCH));
        }

        // Tell current batch about the token we've received.
        // Throws if token changed after being set once, or if we got a non-null token after a commit.
        try {
            uploader.getCurrentBatch().setToken(body.getString(KEY_BATCH), isCommit);
        } catch (BatchingUploader.BatchingUploaderException e) {
            handleRequestError(e);
            return;
        }

        // Will throw if Last-Modified changed when it shouldn't have.
        try {
            uploader.setLastModified(
                    response.normalizedTimestampForHeader(SyncResponse.X_LAST_MODIFIED),
                    isCommit);
        } catch (BatchingUploader.BatchingUploaderException e) {
            handleRequestError(e);
            return;
        }

        // All looks good up to this point, let's process success and failed arrays.
        JSONArray success;
        try {
            success = body.getArray("success");
        } catch (NonArrayJSONException e) {
            handleRequestError(e);
            return;
        }

        if (success != null && !success.isEmpty()) {
            Logger.trace(LOG_TAG, "Successful records: " + success.toString());
            for (Object o : success) {
                try {
                    uploader.recordSucceeded((String) o);
                } catch (ClassCastException e) {
                    Logger.error(LOG_TAG, "Got exception parsing POST success guid.", e);
                    // Not much to be done.
                }
            }
        }
        // GC
        success = null;

        ExtendedJSONObject failed;
        try {
            failed = body.getObject("failed");
        } catch (NonObjectJSONException e) {
            handleRequestError(e);
            return;
        }

        if (failed != null && !failed.object.isEmpty()) {
            Logger.debug(LOG_TAG, "Failed records: " + failed.object.toString());
            for (String guid : failed.keySet()) {
                uploader.recordFailed(guid);
            }
        }
        // GC
        failed = null;

        // And we're done! Let uploader finish up.
        uploader.payloadSucceeded(response, isCommit, isLastPayload);
    }

    @Override
    public void handleRequestFailure(final SyncStorageResponse response) {
        this.handleRequestError(new HTTPFailureException(response));
    }

    @Override
    public void handleRequestError(Exception e) {
        for (String guid : postedRecordGuids) {
            uploader.recordFailed(e, guid);
        }
        // GC
        postedRecordGuids = null;

        if (isLastPayload) {
            uploader.lastPayloadFailed();
        }
    }
}