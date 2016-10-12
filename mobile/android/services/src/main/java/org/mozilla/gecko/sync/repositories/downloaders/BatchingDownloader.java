/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.repositories.downloaders;

import android.support.annotation.Nullable;
import android.support.annotation.VisibleForTesting;

import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.sync.CryptoRecord;
import org.mozilla.gecko.sync.DelayedWorkTracker;
import org.mozilla.gecko.sync.net.SyncResponse;
import org.mozilla.gecko.sync.net.SyncStorageCollectionRequest;
import org.mozilla.gecko.sync.net.SyncStorageResponse;
import org.mozilla.gecko.sync.repositories.Server11Repository;
import org.mozilla.gecko.sync.repositories.Server11RepositorySession;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionFetchRecordsDelegate;

import java.io.UnsupportedEncodingException;
import java.net.URI;
import java.net.URISyntaxException;
import java.net.URLEncoder;
import java.util.Collections;
import java.util.HashSet;
import java.util.Set;

/**
 * Batching Downloader, which implements batching protocol as supported by Sync 1.5.
 *
 * Downloader's batching behaviour is configured via two parameters, obtained from the repository:
 * - Per-batch limit, which specified how many records may be fetched in an individual GET request.
 * - Total limit, which controls number of batch GET requests we will make.
 *
 *
 * Batching is implemented via specifying a 'limit' GET parameter, and looking for an 'offset' token
 * in the response. If offset token is present, this indicates that there are more records than what
 * we've received so far, and we perform an additional fetch. Batching stops when either we hit a total
 * limit, or offset token is no longer present (indicating that we're done).
 *
 * For unlimited repositories (such as passwords), both of these value will be -1. Downloader will not
 * specify a limit parameter in this case, and the response will contain every record available and no
 * offset token, thus fully completing in one go.
 *
 * In between batches, we maintain a Last-Modified timestamp, based off the value return in the header
 * of the first response. Every response will have a Last-Modified header, indicating when the collection
 * was modified last. We pass along this header in our subsequent requests in a X-If-Unmodified-Since
 * header. Server will ensure that our collection did not change while we are batching, if it did it will
 * fail our fetch with a 412 (Consequent Modification) error. Additionally, we perform the same checks
 * locally.
 */
public class BatchingDownloader {
    public static final String LOG_TAG = "BatchingDownloader";

    protected final Server11Repository repository;
    private final Server11RepositorySession repositorySession;
    private final DelayedWorkTracker workTracker = new DelayedWorkTracker();
    // Used to track outstanding requests, so that we can abort them as needed.
    @VisibleForTesting
    protected final Set<SyncStorageCollectionRequest> pending = Collections.synchronizedSet(new HashSet<SyncStorageCollectionRequest>());
    /* @GuardedBy("this") */ private String lastModified;
    /* @GuardedBy("this") */ private long numRecords = 0;

    public BatchingDownloader(final Server11Repository repository, final Server11RepositorySession repositorySession) {
        this.repository = repository;
        this.repositorySession = repositorySession;
    }

    @VisibleForTesting
    protected static String flattenIDs(String[] guids) {
        // Consider using Utils.toDelimitedString if and when the signature changes
        // to Collection<String> guids.
        if (guids.length == 0) {
            return "";
        }
        if (guids.length == 1) {
            return guids[0];
        }
        // Assuming 12-char GUIDs. There should be a -1 in there, but we accumulate one comma too many.
        StringBuilder b = new StringBuilder(guids.length * 12 + guids.length);
        for (String guid : guids) {
            b.append(guid);
            b.append(",");
        }
        return b.substring(0, b.length() - 1);
    }

    @VisibleForTesting
    protected void fetchWithParameters(long newer,
                                    long batchLimit,
                                    boolean full,
                                    String sort,
                                    String ids,
                                    SyncStorageCollectionRequest request,
                                    RepositorySessionFetchRecordsDelegate fetchRecordsDelegate)
            throws URISyntaxException, UnsupportedEncodingException {
        if (batchLimit > repository.getDefaultTotalLimit()) {
            throw new IllegalArgumentException("Batch limit should not be greater than total limit");
        }

        request.delegate = new BatchingDownloaderDelegate(this, fetchRecordsDelegate, request,
                newer, batchLimit, full, sort, ids);
        this.pending.add(request);
        request.get();
    }

    @VisibleForTesting
    @Nullable
    protected String encodeParam(String param) throws UnsupportedEncodingException {
        if (param != null) {
            return URLEncoder.encode(param, "UTF-8");
        }
        return null;
    }

    @VisibleForTesting
    protected SyncStorageCollectionRequest makeSyncStorageCollectionRequest(long newer,
                                                  long batchLimit,
                                                  boolean full,
                                                  String sort,
                                                  String ids,
                                                  String offset)
            throws URISyntaxException, UnsupportedEncodingException {
        URI collectionURI = repository.collectionURI(full, newer, batchLimit, sort, ids, encodeParam(offset));
        Logger.debug(LOG_TAG, collectionURI.toString());

        return new SyncStorageCollectionRequest(collectionURI);
    }

    public void fetchSince(long timestamp, RepositorySessionFetchRecordsDelegate fetchRecordsDelegate) {
        this.fetchSince(timestamp, null, fetchRecordsDelegate);
    }

    private void fetchSince(long timestamp, String offset,
                           RepositorySessionFetchRecordsDelegate fetchRecordsDelegate) {
        long batchLimit = repository.getDefaultBatchLimit();
        String sort = repository.getDefaultSort();

        try {
            SyncStorageCollectionRequest request = makeSyncStorageCollectionRequest(timestamp,
                    batchLimit, true, sort, null, offset);
            this.fetchWithParameters(timestamp, batchLimit, true, sort, null, request, fetchRecordsDelegate);
        } catch (URISyntaxException | UnsupportedEncodingException e) {
            fetchRecordsDelegate.onFetchFailed(e);
        }
    }

    public void fetch(String[] guids, RepositorySessionFetchRecordsDelegate fetchRecordsDelegate) {
        String ids = flattenIDs(guids);
        String index = "index";

        try {
            SyncStorageCollectionRequest request = makeSyncStorageCollectionRequest(
                    -1, -1, true, index, ids, null);
            this.fetchWithParameters(-1, -1, true, index, ids, request, fetchRecordsDelegate);
        } catch (URISyntaxException | UnsupportedEncodingException e) {
            fetchRecordsDelegate.onFetchFailed(e);
        }
    }

    public Server11Repository getServerRepository() {
        return this.repository;
    }

    public void onFetchCompleted(SyncStorageResponse response,
                                 final RepositorySessionFetchRecordsDelegate fetchRecordsDelegate,
                                 final SyncStorageCollectionRequest request, long newer,
                                 long limit, boolean full, String sort, String ids) {
        removeRequestFromPending(request);

        // When we process our first request, we get back a X-Last-Modified header indicating when collection was modified last.
        // We pass it to the server with every subsequent request (if we need to make more) as the X-If-Unmodified-Since header,
        // and server is supposed to ensure that this pre-condition is met, and fail our request with a 412 error code otherwise.
        // So, if all of this happens, these checks should never fail.
        // However, we also track this header in client side, and can defensively validate against it here as well.
        final String currentLastModifiedTimestamp = response.lastModified();
        Logger.debug(LOG_TAG, "Last modified timestamp " + currentLastModifiedTimestamp);

        // Sanity check. We also did a null check in delegate before passing it into here.
        if (currentLastModifiedTimestamp == null) {
            this.abort(fetchRecordsDelegate, "Last modified timestamp is missing");
            return;
        }

        final boolean lastModifiedChanged;
        synchronized (this) {
            if (this.lastModified == null) {
                // First time seeing last modified timestamp.
                this.lastModified = currentLastModifiedTimestamp;
            }
            lastModifiedChanged = !this.lastModified.equals(currentLastModifiedTimestamp);
        }

        if (lastModifiedChanged) {
            this.abort(fetchRecordsDelegate, "Last modified timestamp has changed unexpectedly");
            return;
        }

        final boolean hasNotReachedLimit;
        synchronized (this) {
            this.numRecords += response.weaveRecords();
            hasNotReachedLimit = this.numRecords < repository.getDefaultTotalLimit();
        }

        final String offset = response.weaveOffset();
        final SyncStorageCollectionRequest newRequest;
        try {
            newRequest = makeSyncStorageCollectionRequest(newer,
                    limit, full, sort, ids, offset);
        } catch (final URISyntaxException | UnsupportedEncodingException e) {
            this.workTracker.delayWorkItem(new Runnable() {
                @Override
                public void run() {
                    Logger.debug(LOG_TAG, "Delayed onFetchCompleted running.");
                    fetchRecordsDelegate.onFetchFailed(e);
                }
            });
            return;
        }

        if (offset != null && hasNotReachedLimit) {
            try {
                this.fetchWithParameters(newer, limit, full, sort, ids, newRequest, fetchRecordsDelegate);
            } catch (final URISyntaxException | UnsupportedEncodingException e) {
                this.workTracker.delayWorkItem(new Runnable() {
                    @Override
                    public void run() {
                        Logger.debug(LOG_TAG, "Delayed onFetchCompleted running.");
                        fetchRecordsDelegate.onFetchFailed(e, null);
                    }
                });
            }
            return;
        }

        final long normalizedTimestamp = response.normalizedTimestampForHeader(SyncResponse.X_LAST_MODIFIED);
        Logger.debug(LOG_TAG, "Fetch completed. Timestamp is " + normalizedTimestamp);

        this.workTracker.delayWorkItem(new Runnable() {
            @Override
            public void run() {
                Logger.debug(LOG_TAG, "Delayed onFetchCompleted running.");
                fetchRecordsDelegate.onFetchCompleted(normalizedTimestamp);
            }
        });
    }

    public void onFetchFailed(final Exception ex,
                              final RepositorySessionFetchRecordsDelegate fetchRecordsDelegate,
                              final SyncStorageCollectionRequest request) {
        removeRequestFromPending(request);
        this.workTracker.delayWorkItem(new Runnable() {
            @Override
            public void run() {
                Logger.debug(LOG_TAG, "Running onFetchFailed.");
                fetchRecordsDelegate.onFetchFailed(ex);
            }
        });
    }

    public void onFetchedRecord(CryptoRecord record,
                                RepositorySessionFetchRecordsDelegate fetchRecordsDelegate) {
        this.workTracker.incrementOutstanding();
        try {
            fetchRecordsDelegate.onFetchedRecord(record);
        } catch (Exception ex) {
            Logger.warn(LOG_TAG, "Got exception calling onFetchedRecord with WBO.", ex);
            throw new RuntimeException(ex);
        } finally {
            this.workTracker.decrementOutstanding();
        }
    }

    private void removeRequestFromPending(SyncStorageCollectionRequest request) {
        if (request == null) {
            return;
        }
        this.pending.remove(request);
    }

    @VisibleForTesting
    protected void abortRequests() {
        this.repositorySession.abort();
        synchronized (this.pending) {
            for (SyncStorageCollectionRequest request : this.pending) {
                request.abort();
            }
            this.pending.clear();
        }
    }

    @Nullable
    protected synchronized String getLastModified() {
        return this.lastModified;
    }

    private void abort(final RepositorySessionFetchRecordsDelegate delegate, final String msg) {
        Logger.error(LOG_TAG, msg);
        this.abortRequests();
        this.workTracker.delayWorkItem(new Runnable() {
            @Override
            public void run() {
                Logger.debug(LOG_TAG, "Delayed onFetchCompleted running.");
                delegate.onFetchFailed(
                        new IllegalStateException(msg),
                        null);
            }
        });
    }
}
