/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.repositories.uploaders;

import android.support.annotation.Nullable;

import org.mozilla.gecko.background.common.log.Logger;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.concurrent.ConcurrentLinkedQueue;


/**
 * Keeps track of various meta information about a batch series.
 *
 * NB regarding concurrent access:
 * - this class expects access by possibly different, sequentially running threads.
 * - concurrent access is not supported.
 */
public class BatchMeta {
    private static final String LOG_TAG = "BatchMeta";

    private volatile Boolean inBatchingMode;
    @Nullable private volatile Long lastModified;
    private volatile String token;

    // NB: many of the operations on ConcurrentLinkedQueue are not atomic (toArray, for example),
    // and so use of this queue type is only possible because this class does not support concurrent
    // access.
    private final ConcurrentLinkedQueue<String> successRecordGuids = new ConcurrentLinkedQueue<>();

    BatchMeta(@Nullable Long initialLastModified, Boolean initialInBatchingMode) {
        lastModified = initialLastModified;
        inBatchingMode = initialInBatchingMode;
    }

    String[] getSuccessRecordGuids() {
        // NB: This really doesn't play well with concurrent access.
        final String[] guids = new String[this.successRecordGuids.size()];
        this.successRecordGuids.toArray(guids);
        return guids;
    }

    void recordSucceeded(final String recordGuid) {
        // Sanity check.
        if (recordGuid == null) {
            throw new IllegalStateException("Record guid is unexpectedly null");
        }

        successRecordGuids.add(recordGuid);
    }

    /* package-local */ void setInBatchingMode(boolean inBatchingMode) {
        this.inBatchingMode = inBatchingMode;
    }

    /* package-local */ Boolean getInBatchingMode() {
        return inBatchingMode;
    }

    @Nullable
    protected Long getLastModified() {
        return lastModified;
    }

    void setLastModified(final Long newLastModified, final boolean expectedToChange) throws BatchingUploader.LastModifiedChangedUnexpectedly, BatchingUploader.LastModifiedDidNotChange {
        if (lastModified == null) {
            lastModified = newLastModified;
            return;
        }

        if (!expectedToChange && !lastModified.equals(newLastModified)) {
            Logger.debug(LOG_TAG, "Last-Modified timestamp changed when we didn't expect it");
            throw new BatchingUploader.LastModifiedChangedUnexpectedly();

        } else if (expectedToChange && lastModified.equals(newLastModified)) {
            Logger.debug(LOG_TAG, "Last-Modified timestamp did not change when we expected it to");
            throw new BatchingUploader.LastModifiedDidNotChange();

        } else {
            lastModified = newLastModified;
        }
    }

    @Nullable
    protected String getToken() {
        return token;
    }

    void setToken(final String newToken, boolean isCommit) throws BatchingUploader.TokenModifiedException {
        // Set token once in a batching mode.
        // In a non-batching mode, this.token and newToken will be null, and this is a no-op.
        if (token == null) {
            token = newToken;
            return;
        }

        // Sanity checks.
        if (isCommit) {
            // We expect token to be null when commit payload succeeds.
            if (newToken != null) {
                throw new BatchingUploader.TokenModifiedException();
            } else {
                token = null;
            }
            return;
        }

        // We expect new token to always equal current token for non-commit payloads.
        if (!token.equals(newToken)) {
            throw new BatchingUploader.TokenModifiedException();
        }
    }

    BatchMeta nextBatchMeta() {
        return new BatchMeta(lastModified, inBatchingMode);
    }
}
