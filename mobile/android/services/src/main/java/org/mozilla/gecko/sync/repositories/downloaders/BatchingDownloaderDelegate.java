/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.repositories.downloaders;

import org.mozilla.gecko.background.common.log.Logger;
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

    private BatchingDownloader downloader;
    private RepositorySessionFetchRecordsDelegate fetchRecordsDelegate;
    public SyncStorageCollectionRequest request;
    // Used to pass back to BatchDownloader to start another fetch with these parameters if needed.
    private long newer;
    private long batchLimit;
    private boolean full;
    private String sort;
    private String ids;

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
        return this.downloader.getServerRepository().getAuthHeaderProvider();
    }

    @Override
    public String ifUnmodifiedSince() {
        return this.downloader.getLastModified();
    }

    @Override
    public void handleRequestSuccess(SyncStorageResponse response) {
        Logger.debug(LOG_TAG, "Fetch done.");
        if (response.lastModified() != null) {
            this.downloader.onFetchCompleted(response, this.fetchRecordsDelegate, this.request,
                    this.newer, this.batchLimit, this.full, this.sort, this.ids);
            return;
        }
        this.downloader.onFetchFailed(
                new IllegalStateException("Missing last modified header from response"),
                this.fetchRecordsDelegate,
                this.request);
    }

    @Override
    public void handleRequestFailure(SyncStorageResponse response) {
        this.handleRequestError(new HTTPFailureException(response));
    }

    @Override
    public void handleRequestError(final Exception ex) {
        Logger.warn(LOG_TAG, "Got request error.", ex);
        this.downloader.onFetchFailed(ex, this.fetchRecordsDelegate, this.request);
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
