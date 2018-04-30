/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * vim: ts=4 sw=4 expandtab:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

import org.json.JSONException;
import org.json.JSONObject;

import android.support.annotation.IntDef;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.util.Log;

import org.mozilla.gecko.EventDispatcher;
import org.mozilla.gecko.util.GeckoBundle;
import org.mozilla.gecko.util.EventCallback;

/**
 * The telemetry API gives access to telemetry data of the Gecko runtime.
 */
public final class RuntimeTelemetry {
    private final static String LOGTAG = "GeckoViewTelemetry";
    private final static boolean DEBUG = false;

    private final GeckoRuntime mRuntime;
    private final EventDispatcher mEventDispatcher;

    @Retention(RetentionPolicy.SOURCE)
    @IntDef({ DATASET_BASE, DATASET_EXTENDED })
    public @interface DatasetType {}

    // Match with nsITelemetry.
    /**
     * The base dataset suitable for release builds.
     */
    public static final int DATASET_BASE = 0;
    /**
     * The extended dataset suitable for pre-release builds.
     */
    public static final int DATASET_EXTENDED = 1;

    @IntDef(flag = true,
            value = { SNAPSHOT_HISTOGRAMS, SNAPSHOT_KEYED_HISTOGRAMS,
                      SNAPSHOT_SCALARS, SNAPSHOT_KEYED_SCALARS,
                      SNAPSHOT_ALL })
    public @interface SnapshotType {}

    // Match with GeckoViewTelemetryController.
    /**
     * Adds a "histogram" object entry to the snapshot response.
     */
    public static final int SNAPSHOT_HISTOGRAMS = 1 << 0;
    /**
     * Adds a "keyedHistogram" object entry to the snapshot response.
     */
    public static final int SNAPSHOT_KEYED_HISTOGRAMS = 1 << 1;
    /**
     * Adds a "scalars" object entry to the snapshot response.
     */
    public static final int SNAPSHOT_SCALARS = 1 << 2;
    /**
     * Adds a "keyedScalars" object entry to the snapshot response.
     */
    public static final int SNAPSHOT_KEYED_SCALARS = 1 << 3;
    /**
     * Adds all snapshot types to the response.
     */
    public static final int SNAPSHOT_ALL = (1 << 4) - 1;

    /* package */ RuntimeTelemetry(final @NonNull GeckoRuntime runtime) {
        mRuntime = runtime;
        mEventDispatcher = EventDispatcher.getInstance();
    }

    /**
     * Retrieve all telemetry snapshots.
     *
     * @param dataset The dataset type to retreive.
     *                One of {@link #DATASET_BASE DATASET_*} flags.
     * @param clear Whether the retrieved snapshots should be cleared.
     * @param response Used to return the async response.
     */
    public void getSnapshots(
          final @DatasetType int dataset,
          final boolean clear,
          final @NonNull GeckoResponse<GeckoBundle> response) {
        getSnapshots(SNAPSHOT_ALL, dataset, clear, response);
    }

    /**
     * Retrieve the requested telemetry snapshots.
     *
     * @param types The requested snapshot types.
     *              One or more of {@link #SNAPSHOT_HISTOGRAMS SNAPSHOT_*} flags.
     * @param dataset The dataset type to retreive.
     *                One of {@link #DATASET_BASE DATASET_*} flags.
     * @param clear Whether the retrieved snapshots should be cleared.
     * @param response Used to return the async response.
     */
    public void getSnapshots(
          final @SnapshotType int types,
          final @DatasetType int dataset,
          final boolean clear,
          final @NonNull GeckoResponse<GeckoBundle> response) {
        final GeckoBundle msg = new GeckoBundle(3);
        msg.putInt("types", types);
        msg.putInt("dataset", dataset);
        msg.putBoolean("clear", clear);

        mEventDispatcher.dispatch("GeckoView:TelemetrySnapshots", msg,
            new EventCallback() {
                @Override
                public void sendSuccess(final Object result) {
                    response.respond((GeckoBundle) result);
                }

                @Override
                public void sendError(final Object error) {
                    Log.e(LOGTAG, "getSnapshots failed: " + error);
                    response.respond(null);
                }
            });
    }
}
