/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.repositories.uploaders;

import android.support.annotation.CheckResult;
import android.support.annotation.NonNull;


public class UploaderMeta extends BufferSizeTracker {
    // Will be written and read by different threads.
    /* @GuardedBy("accessLock") */ private volatile boolean isUnlimited = false;

    /* @GuardedBy("accessLock") */ private boolean needsCommit = false;

    public UploaderMeta(@NonNull Object payloadLock, long maxBytes, long maxRecords) {
        super(payloadLock, maxBytes, maxRecords);
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

    @Override
    protected boolean canFitRecordByteDelta(long byteDelta, long recordCount, long byteCount) {
        return isUnlimited || super.canFitRecordByteDelta(byteDelta, recordCount, byteCount);
    }

    UploaderMeta nextUploaderMeta() {
        return new UploaderMeta(accessLock, maxBytes, maxRecords);
    }
}