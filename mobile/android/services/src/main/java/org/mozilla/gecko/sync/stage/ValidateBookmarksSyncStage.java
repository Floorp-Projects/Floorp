/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.stage;


import android.content.Context;
import android.database.Cursor;
import android.os.SystemClock;
import android.support.annotation.Nullable;

import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.sync.MetaGlobalException;
import org.mozilla.gecko.sync.NonObjectJSONException;
import org.mozilla.gecko.sync.SynchronizerConfiguration;
import org.mozilla.gecko.sync.middleware.BufferingMiddlewareRepository;
import org.mozilla.gecko.sync.middleware.MiddlewareRepository;
import org.mozilla.gecko.sync.middleware.storage.MemoryBufferStorage;
import org.mozilla.gecko.sync.repositories.ConfigurableServer15Repository;
import org.mozilla.gecko.sync.repositories.RecordFactory;
import org.mozilla.gecko.sync.repositories.Repository;
import org.mozilla.gecko.sync.repositories.android.BookmarksValidationRepository;
import org.mozilla.gecko.sync.repositories.android.BrowserContractHelpers;
import org.mozilla.gecko.sync.repositories.domain.BookmarkRecordFactory;
import org.mozilla.gecko.sync.repositories.domain.VersionConstants;
import org.mozilla.gecko.sync.telemetry.TelemetryCollector;
import org.mozilla.gecko.sync.telemetry.TelemetryStageCollector;

import java.io.IOException;
import java.net.URISyntaxException;
import java.util.concurrent.TimeUnit;

/**
 * This sync stage runs bookmark validation for including in telemetry. Bookmark validation
 * is fairly costly, so we don't want to run it frequently, and there are a number of
 * checks to make sure it's a good idea to run it.
 *
 * We only even consider running validation once a day, and during that time we
 * make a number of checks to determine if we should run it:
 *
 * - We only run validation (VALIDATION_PROBABILITY * 100)% of the time (based on Math.random())
 * - We must be on nightly (may be relaxed eventually).
 * - You must have have fewer than MAX_BOOKMARKS_COUNT bookmarks in your local database
 * - There must be enough time left in the sync deadline (at least TIME_REQUIRED_TO_VALIDATE ms).
 * - The bookmarks collection must have ran, reported telemetry, and not included an error in
 *   the telemetry stage.
 */
public class ValidateBookmarksSyncStage extends ServerSyncStage {
    protected static final String LOG_TAG = "ValidateBookmarksStage";

    private static final String BOOKMARKS_SORT = "oldest";
    private static final long BOOKMARKS_BATCH_LIMIT = 5000;

    // Maximum number of (local) bookmarks we have where we're still
    // willing to run a validation.
    private static final long MAX_BOOKMARKS_COUNT = 1000;

    // Probability that, given all other requirements are met, we'll run a validation.
    private static final double VALIDATION_PROBABILITY = 0.1;

    // If we have less than this amount of time left, we don't bother validating.
    // Currently 2 minutes, which would be an insane amount of time for a
    // validation to take, but it's better to be conservative here.
    private static final long TIME_REQUIRED_TO_VALIDATE = TimeUnit.MINUTES.toMillis(2);

    // We only even attempt to validate this frequently
    private static final long VALIDATION_INTERVAL = TimeUnit.DAYS.toMillis(1);

    @Override
    protected String getCollection() {
        return "bookmarks";
    }

    @Override
    protected String getEngineName() {
        return "bookmarks";
    }

    @Override
    public Integer getStorageVersion() {
        return VersionConstants.BOOKMARKS_ENGINE_VERSION;
    }

    /**
     * We can't validate without all of the records, so a HWM doesn't make sense.
     *
     * @return HighWaterMark.Disabled
     */
    @Override
    protected HighWaterMark getAllowedToUseHighWaterMark() {
        return HighWaterMark.Disabled;
    }

    /**
     * Full batching is allowed, because we want all of the records.
     *
     * @return MultipleBatches.Enabled
     */
    @Override
    protected MultipleBatches getAllowedMultipleBatches() {
        return MultipleBatches.Enabled;
    }
    @Override
    protected Repository getRemoteRepository() throws URISyntaxException {
        return new ConfigurableServer15Repository(
                getCollection(),
                session.getSyncDeadline(),
                session.config.storageURL(),
                session.getAuthHeaderProvider(),
                session.config.infoCollections,
                session.config.infoConfiguration,
                BOOKMARKS_BATCH_LIMIT,
                BOOKMARKS_SORT,
                getAllowedMultipleBatches(),
                getAllowedToUseHighWaterMark(),
                getRepositoryStateProvider(),
                true,
                false // We never do any storing, so this is irrelevant
        );
    }

    @Override
    protected Repository getLocalRepository() {
        TelemetryStageCollector bookmarkCollector =
                this.telemetryStageCollector.getSyncCollector().collectorFor("bookmarks");
        return new BookmarksValidationRepository(session.getClientsDelegate(), bookmarkCollector);
    }

    @Override
    protected RecordFactory getRecordFactory() {
        return new BookmarkRecordFactory();
    }

    private long getLocalBookmarkRecordCount() {
        final Context context = session.getContext();
        final Cursor cursor = context.getContentResolver().query(
                BrowserContractHelpers.BOOKMARKS_CONTENT_URI,
                new String[] {BrowserContract.Bookmarks._ID},
                null, null, null
        );
        if (cursor == null) {
            return -1;
        }
        try {
            return cursor.getCount();
        } finally {
            cursor.close();
        }
    }

    // This implements the logic described in the header comment.
    private boolean shouldValidate() {
        if (!AppConstants.NIGHTLY_BUILD) {
            return false;
        }
        final long consideredValidationLast = session.config.getLastValidationCheckTimestamp();
        final long now = System.currentTimeMillis();

        if (now - consideredValidationLast < VALIDATION_INTERVAL) {
            return false;
        }

        session.config.persistLastValidationCheckTimestamp(now);

        if (Math.random() > VALIDATION_PROBABILITY) {
            return false;
        }

        // Note that syncDeadline is relative to the elapsedRealtime clock, not `now`.
        final long timeToSyncDeadline = session.getSyncDeadline() - SystemClock.elapsedRealtime();
        if (timeToSyncDeadline < TIME_REQUIRED_TO_VALIDATE) {
            return false;
        }

        // See if we'll have somewhere to store the validation results
        TelemetryCollector syncCollector = telemetryStageCollector.getSyncCollector();
        if (!syncCollector.hasCollectorFor("bookmarks")) {
            return false;
        }

        // And make sure that bookmarks sync didn't hit an error.
        TelemetryStageCollector stageCollector = syncCollector.collectorFor("bookmarks");
        if (stageCollector.error != null) {
            return false;
        }
        long count = getLocalBookmarkRecordCount();
        if (count < 0 || count > MAX_BOOKMARKS_COUNT) {
            return false;
        }

        return true;
    }

    @Override
    protected boolean isEnabled() throws MetaGlobalException {
        if (session == null || session.getContext() == null) {
            return false;
        }

        return super.isEnabled() && shouldValidate();
    }

}
