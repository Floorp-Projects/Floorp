/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.synchronizer;

import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.sync.repositories.NoStoreDelegateException;
import org.mozilla.gecko.sync.repositories.RepositorySession;
import org.mozilla.gecko.sync.repositories.domain.Record;

/**
 * Same as a regular RecordsChannel, except records aren't buffered and are stored when encountered.
 */
public class NonBufferingRecordsChannel extends RecordsChannel {
    private static final String LOG_TAG = "NonBufferingRecordsChannel";

    public NonBufferingRecordsChannel(RepositorySession source, RepositorySession sink, RecordsChannelDelegate delegate) {
        super(source, sink, delegate);
    }

    @Override
    public void onFetchedRecord(Record record) {
        // Don't bother trying to store if we already failed.
        if (fetchFailed.get()) {
            return;
        }

        fetchedCount.incrementAndGet();
        storeAttemptedCount.incrementAndGet();

        try {
            sink.store(record);
        } catch (NoStoreDelegateException e) {
            // Must not happen, bail out.
            throw new IllegalStateException(e);
        }
    }

    @Override
    public void onFetchFailed(Exception ex) {
        // Let non-buffered sessions clean-up their internal state.
        sink.storeIncomplete();
        super.onFetchFailed(ex);
    }

    @Override
    public void onFetchCompleted() {
        // If we already failed, the flow has been finished via onFetchFailed,
        // yet our delegatee might have kept going.
        if (fetchFailed.get()) {
            return;
        }

        // Now we wait for onStoreComplete
        Logger.trace(LOG_TAG, "onFetchCompleted. Calling storeDone.");
        sink.storeDone();
    }
}
