/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.repositories.uploaders;

import android.support.annotation.CheckResult;

import java.util.ArrayList;

/**
 * Owns per-payload record byte and recordGuid buffers.
 */
/* @ThreadSafe */
public class Payload extends BufferSizeTracker {
    // Data of outbound records.
    /* @GuardedBy("accessLock") */ private final ArrayList<byte[]> recordsBuffer = new ArrayList<>();

    // GUIDs of outbound records. Used to fail entire payloads.
    /* @GuardedBy("accessLock") */ private final ArrayList<String> recordGuidsBuffer = new ArrayList<>();

    public Payload(Object payloadLock, long maxBytes, long maxRecords) {
        super(payloadLock, maxBytes, maxRecords);
    }

    @Override
    protected boolean addAndEstimateIfFull(long recordDelta) {
        throw new UnsupportedOperationException();
    }

    @CheckResult
    protected boolean addAndEstimateIfFull(long recordDelta, byte[] recordBytes, String guid) {
        synchronized (accessLock) {
            recordsBuffer.add(recordBytes);
            recordGuidsBuffer.add(guid);
            return super.addAndEstimateIfFull(recordDelta);
        }
    }

    protected ArrayList<byte[]> getRecordsBuffer() {
        synchronized (accessLock) {
            return new ArrayList<>(recordsBuffer);
        }
    }

    protected ArrayList<String> getRecordGuidsBuffer() {
        synchronized (accessLock) {
            return new ArrayList<>(recordGuidsBuffer);
        }
    }

    protected boolean isEmpty() {
        synchronized (accessLock) {
            return recordsBuffer.isEmpty();
        }
    }

    Payload nextPayload() {
        return new Payload(accessLock, maxBytes, maxRecords);
    }
}