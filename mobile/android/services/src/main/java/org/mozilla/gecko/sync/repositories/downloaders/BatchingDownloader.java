/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.repositories.downloaders;

import android.net.Uri;
import android.os.SystemClock;
import android.support.annotation.Nullable;
import android.support.annotation.VisibleForTesting;

import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.sync.CollectionConcurrentModificationException;
import org.mozilla.gecko.sync.CryptoRecord;
import org.mozilla.gecko.sync.DelayedWorkTracker;
import org.mozilla.gecko.sync.SyncDeadlineReachedException;
import org.mozilla.gecko.sync.Utils;
import org.mozilla.gecko.sync.net.AuthHeaderProvider;
import org.mozilla.gecko.sync.net.SyncResponse;
import org.mozilla.gecko.sync.net.SyncStorageCollectionRequest;
import org.mozilla.gecko.sync.net.SyncStorageResponse;
import org.mozilla.gecko.sync.repositories.RepositorySession;
import org.mozilla.gecko.sync.repositories.RepositoryStateProvider;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionFetchRecordsDelegate;

import java.io.UnsupportedEncodingException;
import java.net.URI;
import java.net.URISyntaxException;
import java.util.Collections;
import java.util.HashSet;
import java.util.Set;
import java.util.concurrent.TimeUnit;

/**
 * Batching Downloader implements batching protocol as supported by Sync 1.5.
 *
 * Downloader's batching behaviour is configured via two parameters, obtained from the repository:
 * - Per-batch limit, which specified how many records may be fetched in an individual GET request.
 * - allowMultipleBatches, which determines if downloader is allowed to perform more than one fetch.
 *
 * Batching is implemented via specifying a 'limit' GET parameter, and looking for an 'offset' token
 * in the response. If offset token is present, this indicates that there are more records than what
 * we've received so far, and we perform an additional fetch, if we're allowed to do so by our
 * configuration. Batching stops when offset token is no longer present (indicating that we're done).
 *
 * If we are not allowed to perform multiple batches, we consider batching to be successfully completed
 * after fist fetch request succeeds. Similarly, a trivial case of collection having less records than
 * the batch limit will also successfully complete in one fetch.
 *
 * In between batches, we maintain a Last-Modified timestamp, based off the value returned in the header
 * of the first response. Every response will have a Last-Modified header, indicating when the collection
 * was modified last. We pass along this header in our subsequent requests in a X-If-Unmodified-Since
 * header. Server will ensure that our collection did not change while we are batching, if it did it will
 * fail our fetch with a 412 error. Additionally, we perform the same checks locally.
 */
public class BatchingDownloader {
    public static final String LOG_TAG = "BatchingDownloader";
    private static final String DEFAULT_SORT_ORDER = "index";

    private final RepositorySession repositorySession;
    private final DelayedWorkTracker workTracker = new DelayedWorkTracker();
    private final Uri baseCollectionUri;
    private final long fetchDeadline;
    private final boolean allowMultipleBatches;
    private final boolean keepTrackOfHighWaterMark;

    private RepositoryStateProvider stateProvider;

    /* package-local */ final AuthHeaderProvider authHeaderProvider;

    // Used to track outstanding requests, so that we can abort them as needed.
    @VisibleForTesting
    protected final Set<SyncStorageCollectionRequest> pending = Collections.synchronizedSet(new HashSet<SyncStorageCollectionRequest>());
    /* @GuardedBy("this") */ private String lastModified;

    public BatchingDownloader(
            AuthHeaderProvider authHeaderProvider,
            Uri baseCollectionUri,
            long fetchDeadline,
            boolean allowMultipleBatches,
            boolean keepTrackOfHighWaterMark,
            RepositoryStateProvider stateProvider,
            RepositorySession repositorySession) {
        this.repositorySession = repositorySession;
        this.authHeaderProvider = authHeaderProvider;
        this.baseCollectionUri = baseCollectionUri;
        this.allowMultipleBatches = allowMultipleBatches;
        this.keepTrackOfHighWaterMark = keepTrackOfHighWaterMark;
        this.fetchDeadline = fetchDeadline;
        this.stateProvider = stateProvider;
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
        request.delegate = new BatchingDownloaderDelegate(this, fetchRecordsDelegate, request,
                newer, batchLimit, full, sort, ids);
        this.pending.add(request);
        request.get();
    }

    @VisibleForTesting
    protected SyncStorageCollectionRequest makeSyncStorageCollectionRequest(long newer,
                                                  long batchLimit,
                                                  boolean full,
                                                  String sort,
                                                  String ids,
                                                  String offset)
            throws URISyntaxException, UnsupportedEncodingException {
        final URI collectionURI = buildCollectionURI(baseCollectionUri, full, newer, batchLimit, sort, ids, offset);
        Logger.debug(LOG_TAG, collectionURI.toString());

        return new SyncStorageCollectionRequest(collectionURI);
    }

    public void fetchSince(RepositorySessionFetchRecordsDelegate fetchRecordsDelegate, long timestamp, long batchLimit, String sortOrder, String offset) {
        try {
            SyncStorageCollectionRequest request = makeSyncStorageCollectionRequest(timestamp,
                    batchLimit, true, sortOrder, null, offset);
            this.fetchWithParameters(timestamp, batchLimit, true, sortOrder, null, request, fetchRecordsDelegate);
        } catch (URISyntaxException | UnsupportedEncodingException e) {
            fetchRecordsDelegate.onFetchFailed(e);
        }
    }

    public void fetch(String[] guids, RepositorySessionFetchRecordsDelegate fetchRecordsDelegate) {
        String ids = flattenIDs(guids);

        try {
            SyncStorageCollectionRequest request = makeSyncStorageCollectionRequest(
                    -1, -1, true, DEFAULT_SORT_ORDER, ids, null);
            this.fetchWithParameters(-1, -1, true, DEFAULT_SORT_ORDER, ids, request, fetchRecordsDelegate);
        } catch (URISyntaxException | UnsupportedEncodingException e) {
            fetchRecordsDelegate.onFetchFailed(e);
        }
    }

    public void onFetchCompleted(SyncStorageResponse response,
                                 final RepositorySessionFetchRecordsDelegate fetchRecordsDelegate,
                                 final SyncStorageCollectionRequest request, long newer,
                                 long limit, boolean full, String sort, String ids) {
        removeRequestFromPending(request);

        // When we process our first request, we get back a X-Last-Modified header indicating when
        // collection was modified last. We pass it to the server with every subsequent request
        // (if we need to make more) as the X-If-Unmodified-Since header, and server is supposed to
        // ensure that this pre-condition is met, and fail our request with a 412 error code otherwise.
        // So, if all of this happens, these checks should never fail. However, we also track this
        // header on the client side, and can defensively validate against it here as well.

        // This value won't be null, since we check for this in the delegate.
        final String currentLastModifiedTimestamp = response.lastModified();
        Logger.debug(LOG_TAG, "Last modified timestamp " + currentLastModifiedTimestamp);

        final boolean lastModifiedChanged;
        synchronized (this) {
            if (this.lastModified == null) {
                // First time seeing last modified timestamp.
                this.lastModified = currentLastModifiedTimestamp;
            }
            lastModifiedChanged = !this.lastModified.equals(currentLastModifiedTimestamp);
        }

        // We expected server to fail our request with 412 in case of concurrent modifications, so
        // this is unexpected. However, let's treat this case just as if we received a 412.
        if (lastModifiedChanged) {
            this.handleFetchFailed(
                    fetchRecordsDelegate,
                    new CollectionConcurrentModificationException()
            );
            return;
        }

        // If we can (or must) stop batching at this point, let the delegate know that we're all done!
        final String offset = response.weaveOffset();
        if (offset == null || !allowMultipleBatches) {
            final long normalizedTimestamp = response.normalizedTimestampForHeader(SyncResponse.X_LAST_MODIFIED);
            Logger.debug(LOG_TAG, "Fetch completed. Timestamp is " + normalizedTimestamp);

            // This isn't great, but shouldn't be too problematic - but do see notes below.
            // Failing to reset a resume context after we're done with batching means that on next
            // sync we'll erroneously try to resume downloading. If resume proceeds, we will fetch
            // from an older timestamp, but offset by the amount of records we've fetched prior.
            // Since we're diligent about setting a X-I-U-S header, any remote collection changes
            // will be caught and we'll receive a 412.
            if (!BatchingDownloaderController.resetResumeContextAndCommit(this.stateProvider)) {
                Logger.warn(LOG_TAG, "Failed to reset resume context while completing a batch");
            }

            this.workTracker.delayWorkItem(new Runnable() {
                @Override
                public void run() {
                    Logger.debug(LOG_TAG, "Delayed onFetchCompleted running.");
                    fetchRecordsDelegate.onFetchCompleted(normalizedTimestamp);
                }
            });
            return;
        }

        // This is unfortunate, but largely just means that in case we need to resume later on, it
        // either won't be possible (and we'll fetch w/o resuming), or won't be as efficient (i.e.
        // we'll download more records than necessary).
        if (BatchingDownloaderController.isResumeContextSet(this.stateProvider)) {
            if (!BatchingDownloaderController.updateResumeContextAndCommit(this.stateProvider, offset)) {
                Logger.warn(LOG_TAG, "Failed to update resume context while processing a batch.");
            }
        } else {
            if (!BatchingDownloaderController.setInitialResumeContextAndCommit(this.stateProvider, offset, newer, sort)) {
                Logger.warn(LOG_TAG, "Failed to set initial resume context while processing a batch.");
            }
        }

        // We need to make another batching request!
        // Let the delegate know that a batch fetch just completed before we proceed.
        // This operation needs to run after every call to onFetchedRecord for this batch has been
        // processed, hence the delayWorkItem call.
        this.workTracker.delayWorkItem(new Runnable() {
            @Override
            public void run() {
                Logger.debug(LOG_TAG, "Running onBatchCompleted.");
                fetchRecordsDelegate.onBatchCompleted();
            }
        });

        // Should we proceed, however? Do we have enough time?
        if (!mayProceedWithBatching(fetchDeadline)) {
            this.handleFetchFailed(fetchRecordsDelegate, new SyncDeadlineReachedException());
            return;
        }

        // Create and execute new batch request.
        try {
            final SyncStorageCollectionRequest newRequest = makeSyncStorageCollectionRequest(newer,
                    limit, full, sort, ids, offset);
            this.fetchWithParameters(newer, limit, full, sort, ids, newRequest, fetchRecordsDelegate);
        } catch (final URISyntaxException | UnsupportedEncodingException e) {
            if (!this.stateProvider.commit()) {
                Logger.warn(LOG_TAG, "Failed to commit repository state while handling request creation error");
            }
            this.workTracker.delayWorkItem(new Runnable() {
                @Override
                public void run() {
                    Logger.debug(LOG_TAG, "Delayed onFetchCompleted running.");
                    fetchRecordsDelegate.onFetchFailed(e);
                }
            });
        }
    }

    private void handleFetchFailed(final RepositorySessionFetchRecordsDelegate fetchRecordsDelegate,
                                  final Exception ex) {
        handleFetchFailed(fetchRecordsDelegate, ex, null);
    }

    /* package-local */ void handleFetchFailed(final RepositorySessionFetchRecordsDelegate fetchRecordsDelegate,
                              final Exception ex,
                              @Nullable final SyncStorageCollectionRequest request) {
        this.removeRequestFromPending(request);
        this.abortRequests();

        // Resume context is not discarded if we failed because of reaching our deadline. In this case,
        // we keep it allowing us to resume our download exactly where we left off.
        // Discard resume context for all other failures: 412 (concurrent modification), HTTP errors, ...
        if (!(ex instanceof SyncDeadlineReachedException)) {
            // Failing to reset context means that we will try to resume once we re-sync current stage.
            // This won't affect X-I-U-S logic in case of 412 (it's set separately from resume context),
            // and same notes apply after failing to reset context in onFetchCompleted (see above).
            if (!BatchingDownloaderController.resetResumeContextAndCommit(stateProvider)) {
                Logger.warn(LOG_TAG, "Failed to reset resume context while processing a non-deadline exception");
            }
        } else {
            // Failing to commit the context here means that we didn't commit the latest high-water-mark,
            // and won't be as efficient once we re-sync. That is, we might download more records than necessary.
            if (!this.stateProvider.commit()) {
                Logger.warn(LOG_TAG, "Failed to commit resume context while processing a deadline exception");
            }
        }

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
            // NB: changes to stateProvider are committed in either onFetchCompleted or handleFetchFailed.
            if (this.keepTrackOfHighWaterMark) {
                this.stateProvider.setLong(RepositoryStateProvider.KEY_HIGH_WATER_MARK, record.lastModified);
            }
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

    private static boolean mayProceedWithBatching(long deadline) {
        // For simplicity, allow batching to proceed if there's at least a minute left for the sync.
        // This should be enough to fetch and process records in the batch.
        final long timeLeft = deadline - SystemClock.elapsedRealtime();
        return timeLeft > TimeUnit.MINUTES.toMillis(1);
    }

    @VisibleForTesting
    public static URI buildCollectionURI(Uri baseCollectionUri, boolean full, long newer, long limit, String sort, String ids, String offset) throws URISyntaxException {
        Uri.Builder uriBuilder = baseCollectionUri.buildUpon();

        if (full) {
            uriBuilder.appendQueryParameter("full", "1");
        }

        if (newer >= 0) {
            // Translate local millisecond timestamps into server decimal seconds.
            String newerString = Utils.millisecondsToDecimalSecondsString(newer);
            uriBuilder.appendQueryParameter("newer", newerString);
        }
        if (limit > 0) {
            uriBuilder.appendQueryParameter("limit", Long.toString(limit));
        }
        if (sort != null) {
            uriBuilder.appendQueryParameter("sort", sort); // We trust these values.
        }
        if (ids != null) {
            uriBuilder.appendQueryParameter("ids", ids); // We trust these values.
        }
        if (offset != null) {
            // Offset comes straight out of HTTP headers and it is the responsibility of the caller to URI-escape it.
            uriBuilder.appendQueryParameter("offset", offset);
        }

        return new URI(uriBuilder.build().toString());
    }
}
