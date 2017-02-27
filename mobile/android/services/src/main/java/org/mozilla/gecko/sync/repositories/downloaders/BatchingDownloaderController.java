/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.repositories.downloaders;

import android.support.annotation.CheckResult;
import android.util.Log;

import org.mozilla.gecko.sync.repositories.RepositoryStateProvider;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionFetchRecordsDelegate;

/**
 * Encapsulates logic for resuming batching downloads.
 *
 * It's possible to resume a batching download if we have an offset token and the context in which
 * we obtained the offset token did not change. Namely, we ensure that `since` and `order` parameters
 * remain the same if offset is being used. See Bug 1330839 for a discussion on this.
 *
 * @author grisha
 */
public class BatchingDownloaderController {
    private final static String LOG_TAG = "BatchingDownloaderCtrl";

    private BatchingDownloaderController() {}

    private static class ResumeContext {
        private final String offset;
        private final Long since;
        private final String order;

        private ResumeContext(String offset, Long since, String order) {
            this.offset = offset;
            this.since = since;
            this.order = order;
        }
    }

    private static ResumeContext getResumeContext(RepositoryStateProvider stateProvider, Long since, String order) {
        // Build a "default" context around passed-in values if no context is available.
        if (!isResumeContextSet(stateProvider)) {
            return new ResumeContext(null, since, order);
        }

        final String offset = stateProvider.getString(RepositoryStateProvider.KEY_OFFSET);
        final Long offsetSince = stateProvider.getLong(RepositoryStateProvider.KEY_OFFSET_SINCE);
        final String offsetOrder = stateProvider.getString(RepositoryStateProvider.KEY_OFFSET_ORDER);

        // If context is still valid, we can use it!
        if (order.equals(offsetOrder)) {
            return new ResumeContext(offset, offsetSince, offsetOrder);
        }

        // Build a "default" context around passed-in values.
        return new ResumeContext(null, since, order);
    }

    /**
     * Resumes a fetch if there is an offset present, and offset's context matches provided values.
     * Otherwise, performs a regular fetch.
     */
    public static void resumeFetchSinceIfPossible(
            BatchingDownloader downloader,
            RepositoryStateProvider stateProvider,
            RepositorySessionFetchRecordsDelegate fetchRecordsDelegate,
            long since, long limit, String order) {
        ResumeContext resumeContext = getResumeContext(stateProvider, since, order);

        downloader.fetchSince(
                fetchRecordsDelegate,
                resumeContext.since,
                limit,
                resumeContext.order,
                resumeContext.offset
        );
    }

    @CheckResult
    /* package-local */ static boolean setInitialResumeContextAndCommit(RepositoryStateProvider stateProvider, String offset, long since, String order) {
        if (isResumeContextSet(stateProvider)) {
            throw new IllegalStateException("Not allowed to set resume context more than once. Use update instead.");
        }

        return stateProvider
                .setString(RepositoryStateProvider.KEY_OFFSET, offset)
                .setLong(RepositoryStateProvider.KEY_OFFSET_SINCE, since)
                .setString(RepositoryStateProvider.KEY_OFFSET_ORDER, order)
                .commit();
    }

    @CheckResult
    /* package-local */ static boolean updateResumeContextAndCommit(RepositoryStateProvider stateProvider, String offset) {
        if (!isResumeContextSet(stateProvider)) {
            throw new IllegalStateException("Tried to update resume context before it was set.");
        }

        return stateProvider
                .setString(RepositoryStateProvider.KEY_OFFSET, offset)
                .commit();
    }

    @CheckResult
    /* package-local */ static boolean resetResumeContextAndCommit(RepositoryStateProvider stateProvider) {
        return stateProvider
                .clear(RepositoryStateProvider.KEY_OFFSET)
                .clear(RepositoryStateProvider.KEY_OFFSET_SINCE)
                .clear(RepositoryStateProvider.KEY_OFFSET_ORDER)
                .commit();
    }

    /*package-local */ static boolean isResumeContextSet(RepositoryStateProvider stateProvider) {
        final String offset = stateProvider.getString(RepositoryStateProvider.KEY_OFFSET);
        final Long offsetSince = stateProvider.getLong(RepositoryStateProvider.KEY_OFFSET_SINCE);
        final String offsetOrder = stateProvider.getString(RepositoryStateProvider.KEY_OFFSET_ORDER);

        return offset != null && offsetSince != null && offsetOrder != null;
    }
}
