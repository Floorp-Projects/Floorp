/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.repositories.downloaders;

import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.sync.CollectionConcurrentModificationException;
import org.mozilla.gecko.sync.CryptoRecord;
import org.mozilla.gecko.sync.HTTPFailureException;
import org.mozilla.gecko.sync.crypto.KeyBundle;
import org.mozilla.gecko.sync.net.AuthHeaderProvider;
import org.mozilla.gecko.sync.net.SyncStorageCollectionRequest;
import org.mozilla.gecko.sync.net.SyncStorageResponse;
import org.mozilla.gecko.sync.net.WBOCollectionRequestDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionFetchRecordsDelegate;


/**
 * Delegate that gets passed into fetch methods to handle server response from fetch.
 */
public class BatchingDownloaderDelegate extends WBOCollectionRequestDelegate {
    public static final String LOG_TAG = "BatchingDownloaderDelegate";

    private final BatchingDownloader downloader;
    private final RepositorySessionFetchRecordsDelegate fetchRecordsDelegate;
    public final SyncStorageCollectionRequest request;
    // Used to pass back to BatchDownloader to start another fetch with these parameters if needed.
    private final long newer;
    private final long batchLimit;
    private final boolean full;
    private final String sort;
    private final String ids;

    public BatchingDownloaderDelegate(final BatchingDownloader downloader,
                                      final RepositorySessionFetchRecordsDelegate fetchRecordsDelegate,
                                      final SyncStorageCollectionRequest request, long newer,
                                      long batchLimit, boolean full, String sort, String ids) {
        this.downloader = downloader;
        this.fetchRecordsDelegate = fetchRecordsDelegate;
        this.request = request;
        this.newer = newer;
        this.batchLimit = batchLimit;
        this.full = full;
        this.sort = sort;
        this.ids = ids;
    }

    @Override
    public AuthHeaderProvider getAuthHeaderProvider() {
        return this.downloader.authHeaderProvider;
    }

    @Override
    public String ifUnmodifiedSince() {
        return this.downloader.getLastModified();
    }

    @Override
    public void handleRequestSuccess(SyncStorageResponse response) {
        Logger.debug(LOG_TAG, "Fetch done.");

        // Sanity check.
        if (response.lastModified() == null) {
            this.downloader.handleFetchFailed(
                    this.fetchRecordsDelegate,
                    new IllegalStateException("Missing last modified header from response"),
                    this.request
            );
            return;
        }

        this.downloader.onFetchCompleted(response, this.fetchRecordsDelegate, this.request,
                this.newer, this.batchLimit, this.full, this.sort, this.ids);
    }

    @Override
    public void handleRequestFailure(SyncStorageResponse response) {
        Logger.warn(LOG_TAG, "Got a non-success response.");
        // Handle concurrent modification errors separately.
        final Exception ex;
        if (response.getStatusCode() == 412) {
            ex = new CollectionConcurrentModificationException();
        } else {
            ex = new HTTPFailureException(response);
        }
        this.handleRequestError(ex);
    }

    @Override
    public void handleRequestError(final Exception ex) {
        Logger.warn(LOG_TAG, "Got request error.", ex);
        this.downloader.handleFetchFailed(this.fetchRecordsDelegate, ex, this.request);
    }

    @Override
    public void handleWBO(CryptoRecord record) {
        this.downloader.onFetchedRecord(record, this.fetchRecordsDelegate);
    }

    @Override
    public KeyBundle keyBundle() {
        return null;
    }
}
