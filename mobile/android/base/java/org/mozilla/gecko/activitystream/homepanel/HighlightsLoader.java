/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.activitystream.homepanel;

import android.content.Context;
import android.database.ContentObserver;
import android.database.Cursor;
import android.os.SystemClock;
import android.support.annotation.WorkerThread;
import android.support.v4.content.AsyncTaskLoader;

import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.activitystream.ranking.HighlightsRanking;
import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.activitystream.homepanel.model.Highlight;
import org.mozilla.gecko.util.ThreadUtils;

import java.util.Collections;
import java.util.List;

/**
 * Loader implementation for loading a list of ranked highlights from the database.
 */
/* package-private */ class HighlightsLoader extends AsyncTaskLoader<List<Highlight>> {
    private static final String TELEMETRY_HISTOGRAM_ACTIVITY_STREAM_HIGHLIGHTS = "FENNEC_ACTIVITY_STREAM_HIGHLIGHTS_LOADER_TIME_MS";

    private final Context context;
    private final int candidatesLimit;
    private final int highlightsLimit;
    private final ContentObserver observer;

    /**
     * Create a new loader for loading and ranking highlights.
     *
     * @param candidatesLimit Number of database entries to consider and rank for finding highlights.
     * @param highlightsLimit Number of highlights that should be returned (max).
     */
    /* package-private */ HighlightsLoader(Context context, int candidatesLimit, int highlightsLimit) {
        super(context);

        this.context = context;
        this.candidatesLimit = candidatesLimit;
        this.highlightsLimit = highlightsLimit;
        this.observer = new ForceLoadContentObserver();
    }

    @Override
    public List<Highlight> loadInBackground() {
        final long startTime = SystemClock.uptimeMillis();

        final Cursor candidatesCursor = BrowserDB.from(context)
                .getHighlightCandidates(context.getContentResolver(), candidatesLimit);

        if (candidatesCursor == null) {
            return Collections.emptyList();
        }

        try {
            // From now on get notified about content updates and reload data - until loader is reset.
            enableContentUpdates();

            final List<Highlight> highlights = HighlightsRanking.rank(candidatesCursor, highlightsLimit);
            forceLoadHighlightMetadata(highlights); // force load now that we have a short list of the data.

            addToPerformanceHistogram(startTime);

            return highlights;
        } finally {
            candidatesCursor.close();
        }
    }

    /**
     * Optimization: we force the lazily-loaded metadata to load while we're running on this background thread
     * rather than letting it block the UI thread when this data is being used in the UI.
     */
    @WorkerThread
    private static void forceLoadHighlightMetadata(final List<Highlight> highlights) {
        ThreadUtils.assertNotOnUiThread();
        for (final Highlight highlight : highlights) {
            highlight.getMetadataSlow();
        }
    }

    private void addToPerformanceHistogram(long startTime) {
        final long took = SystemClock.uptimeMillis() - startTime;

        Telemetry.addToHistogram(TELEMETRY_HISTOGRAM_ACTIVITY_STREAM_HIGHLIGHTS, (int) Math.min(took, Integer.MAX_VALUE));
    }

    @Override
    protected void onReset() {
        disableContentUpdates();
    }

    @Override
    protected void onStartLoading() {
        forceLoad();
    }

    private void enableContentUpdates() {
        context.getContentResolver()
                .registerContentObserver(BrowserContract.AUTHORITY_URI, true, observer);
    }

    private void disableContentUpdates() {
        context.getContentResolver()
                .unregisterContentObserver(observer);
    }
}
