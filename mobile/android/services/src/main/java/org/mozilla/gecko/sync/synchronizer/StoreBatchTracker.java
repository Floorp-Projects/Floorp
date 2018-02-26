/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.synchronizer;

import java.util.ArrayList;
import java.util.concurrent.ConcurrentLinkedQueue;
import java.util.concurrent.atomic.AtomicInteger;

/**
 * This class encapsulates most of the logic for recording telemetry information about outgoing
 * batches.
 */
public class StoreBatchTracker {
    private final AtomicInteger currentStoreBatchAttempted = new AtomicInteger();
    private final AtomicInteger currentStoreBatchAccepted = new AtomicInteger();
    private final AtomicInteger currentStoreBatchFailed = new AtomicInteger();
    private final ConcurrentLinkedQueue<Batch> completedBatches = new ConcurrentLinkedQueue<>();

    public static class Batch {
        public final int sent;
        public final int failed;
        public Batch(final int sent, final int failed) {
            this.sent = sent;
            this.failed = failed;
        }
    }

    /* package-local */ void reset() {
        currentStoreBatchFailed.set(0);
        currentStoreBatchAccepted.set(0);
        currentStoreBatchAttempted.set(0);
        completedBatches.clear();
    }

    private boolean haveUncommittedBatchData() {
        return currentStoreBatchAttempted.get() != 0 ||
                currentStoreBatchAccepted.get() != 0 ||
                currentStoreBatchFailed.get() != 0;
    }

    /* package-local */ void onBatchFinished() {
        // Not actually thread safe, but should be good enough for telemetry info. (Especially
        // since we shouldn't be completing batches for the same channel on more than one thread)
        final int sent = currentStoreBatchAttempted.getAndSet(0);
        final int knownFailed = currentStoreBatchFailed.getAndSet(0);
        final int knownSucceeded = currentStoreBatchAccepted.getAndSet(0);

        // These might be different if we "forced" a failure due to an error unrelated to uploading
        // a record.
        final int failed = Math.max(knownFailed, sent - knownSucceeded);

        completedBatches.add(new Batch(sent, failed));
    }

    /* package-local */ void onBatchFailed() {
        if (currentStoreBatchFailed.get() == 0 && currentStoreBatchAccepted.get() == currentStoreBatchAttempted.get()) {
            // Force the failure. It's unclear if this ever can happen for cases where we care about what
            // is inside completedBatches (cases where we're uploading).
            currentStoreBatchFailed.incrementAndGet();
        }
        onBatchFinished();
    }

    /* package-local */ void onRecordStoreFailed() {
        currentStoreBatchFailed.incrementAndGet();
    }

    /* package-local */ void onRecordStoreSucceeded(int count) {
        currentStoreBatchAccepted.addAndGet(count);
    }

    /* package-local */ void onRecordStoreAttempted() {
        currentStoreBatchAttempted.incrementAndGet();
    }

    // Note that this finishes the current batch (if any exists).
    /* package-local */ ArrayList<Batch> getStoreBatches() {
        if (haveUncommittedBatchData()) {
            onBatchFinished();
        }
        return new ArrayList<>(completedBatches);
    }
}
