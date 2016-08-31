/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.repositories.uploaders;

import android.support.annotation.CheckResult;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;

import org.mozilla.gecko.background.common.log.Logger;

import java.util.ArrayList;
import java.util.List;

import org.mozilla.gecko.sync.repositories.uploaders.BatchingUploader.TokenModifiedException;
import org.mozilla.gecko.sync.repositories.uploaders.BatchingUploader.LastModifiedChangedUnexpectedly;
import org.mozilla.gecko.sync.repositories.uploaders.BatchingUploader.LastModifiedDidNotChange;

/**
 * Keeps track of token, Last-Modified value and GUIDs of succeeded records.
 */
/* @ThreadSafe */
public class BatchMeta extends BufferSizeTracker {
    private static final String LOG_TAG = "BatchMeta";

    // Will be set once first payload upload succeeds. We don't expect this to change until we
    // commit the batch, and which point it must change.
    /* @GuardedBy("this") */ private Long lastModified;

    // Will be set once first payload upload succeeds. We don't expect this to ever change until
    // a commit succeeds, at which point this gets set to null.
    /* @GuardedBy("this") */ private String token;

    /* @GuardedBy("accessLock") */ private boolean isUnlimited = false;

    // Accessed by synchronously running threads.
    /* @GuardedBy("accessLock") */ private final List<String> successRecordGuids = new ArrayList<>();

    /* @GuardedBy("accessLock") */ private boolean needsCommit = false;

    protected final Long collectionLastModified;

    public BatchMeta(@NonNull Object payloadLock, long maxBytes, long maxRecords, @Nullable Long collectionLastModified) {
        super(payloadLock, maxBytes, maxRecords);
        this.collectionLastModified = collectionLastModified;
    }

    protected void setIsUnlimited(boolean isUnlimited) {
        synchronized (accessLock) {
            this.isUnlimited = isUnlimited;
        }
    }

    @Override
    protected boolean canFit(long recordDeltaByteCount) {
        synchronized (accessLock) {
            return isUnlimited || super.canFit(recordDeltaByteCount);
        }
    }

    @Override
    @CheckResult
    protected boolean addAndEstimateIfFull(long recordDeltaByteCount) {
        synchronized (accessLock) {
            needsCommit = true;
            boolean isFull = super.addAndEstimateIfFull(recordDeltaByteCount);
            return !isUnlimited && isFull;
        }
    }

    protected boolean needToCommit() {
        synchronized (accessLock) {
            return needsCommit;
        }
    }

    protected synchronized String getToken() {
        return token;
    }

    protected synchronized void setToken(final String newToken, boolean isCommit) throws TokenModifiedException {
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
                throw new TokenModifiedException();
            } else {
                token = null;
            }
            return;
        }

        // We expect new token to always equal current token for non-commit payloads.
        if (!token.equals(newToken)) {
            throw new TokenModifiedException();
        }
    }

    protected synchronized Long getLastModified() {
        if (lastModified == null) {
            return collectionLastModified;
        }
        return lastModified;
    }

    protected synchronized void setLastModified(final Long newLastModified, final boolean expectedToChange) throws LastModifiedChangedUnexpectedly, LastModifiedDidNotChange {
        if (lastModified == null) {
            lastModified = newLastModified;
            return;
        }

        if (!expectedToChange && !lastModified.equals(newLastModified)) {
            Logger.debug(LOG_TAG, "Last-Modified timestamp changed when we didn't expect it");
            throw new LastModifiedChangedUnexpectedly();

        } else if (expectedToChange && lastModified.equals(newLastModified)) {
            Logger.debug(LOG_TAG, "Last-Modified timestamp did not change when we expected it to");
            throw new LastModifiedDidNotChange();

        } else {
            lastModified = newLastModified;
        }
    }

    protected ArrayList<String> getSuccessRecordGuids() {
        synchronized (accessLock) {
            return new ArrayList<>(this.successRecordGuids);
        }
    }

    protected void recordSucceeded(final String recordGuid) {
        // Sanity check.
        if (recordGuid == null) {
            throw new IllegalStateException();
        }

        synchronized (accessLock) {
            successRecordGuids.add(recordGuid);
        }
    }

    @Override
    protected boolean canFitRecordByteDelta(long byteDelta, long recordCount, long byteCount) {
        return isUnlimited || super.canFitRecordByteDelta(byteDelta, recordCount, byteCount);
    }

    @Override
    protected void reset() {
        synchronized (accessLock) {
            super.reset();
            token = null;
            lastModified = null;
            successRecordGuids.clear();
            needsCommit = false;
        }
    }
}