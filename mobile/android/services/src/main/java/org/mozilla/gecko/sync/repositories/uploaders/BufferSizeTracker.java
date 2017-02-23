/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.repositories.uploaders;

import android.support.annotation.CallSuper;
import android.support.annotation.CheckResult;

/**
 * Implements functionality shared by BatchMeta and Payload objects, namely:
 * - keeping track of byte and record counts
 * - incrementing those counts when records are added
 * - checking if a record can fit
 */
/* @ThreadSafe */
public abstract class BufferSizeTracker {
    protected final Object accessLock;

    /* @GuardedBy("accessLock") */ private long byteCount = BatchingUploader.PER_PAYLOAD_OVERHEAD_BYTE_COUNT;
    /* @GuardedBy("accessLock") */ private long recordCount = 0;
    /* @GuardedBy("accessLock") */ private Long smallestRecordByteCount;

    protected final long maxBytes;
    protected final long maxRecords;

    public BufferSizeTracker(Object accessLock, long maxBytes, long maxRecords) {
        this.accessLock = accessLock;
        this.maxBytes = maxBytes;
        this.maxRecords = maxRecords;
    }

    @CallSuper
    protected boolean canFit(long recordDeltaByteCount) {
        synchronized (accessLock) {
            return canFitRecordByteDelta(recordDeltaByteCount, recordCount, byteCount);
        }
    }

    protected boolean isEmpty() {
        synchronized (accessLock) {
            return recordCount == 0;
        }
    }

    /**
     * Adds a record and returns a boolean indicating whether batch is estimated to be full afterwards.
     */
    @CheckResult
    protected boolean addAndEstimateIfFull(long recordDeltaByteCount) {
        synchronized (accessLock) {
            // Sanity check. Calling this method when buffer won't fit the record is an error.
            if (!canFitRecordByteDelta(recordDeltaByteCount, recordCount, byteCount)) {
                throw new IllegalStateException("Buffer size exceeded");
            }

            byteCount += recordDeltaByteCount;
            recordCount += 1;

            if (smallestRecordByteCount == null || smallestRecordByteCount > recordDeltaByteCount) {
                smallestRecordByteCount = recordDeltaByteCount;
            }

            // See if we're full or nearly full after adding a record.
            // We're halving smallestRecordByteCount because we're erring
            // on the side of "can hopefully fit". We're trying to upload as soon as we know we
            // should, but we also need to be mindful of minimizing total number of uploads we make.
            return !canFitRecordByteDelta(smallestRecordByteCount / 2, recordCount, byteCount);
        }
    }

    protected long getByteCount() {
        synchronized (accessLock) {
            // Ensure we account for payload overhead twice when the batch is empty.
            // Payload overhead is either RECORDS_START ("[") or RECORDS_END ("]"),
            // and for an empty payload we need account for both ("[]").
            if (recordCount == 0) {
                return byteCount + BatchingUploader.PER_PAYLOAD_OVERHEAD_BYTE_COUNT;
            }
            return byteCount;
        }
    }

    protected long getRecordCount() {
        synchronized (accessLock) {
            return recordCount;
        }
    }

    @CallSuper
    protected boolean canFitRecordByteDelta(long byteDelta, long recordCount, long byteCount) {
        return recordCount < maxRecords
                && (byteCount + byteDelta) <= maxBytes;
    }
}