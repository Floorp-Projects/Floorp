/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * vim: ts=4 sw=4 expandtab:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview;

import android.support.annotation.NonNull;
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

    /* package */ RuntimeTelemetry(final @NonNull GeckoRuntime runtime) {
        mRuntime = runtime;
        mEventDispatcher = EventDispatcher.getInstance();
    }

    /**
     * Retrieve all telemetry snapshots.
     * The response bundle will contain following snapshots:
     * <ul>
     * <li>histograms</li>
     * <li>keyedHistograms</li>
     * <li>scalars</li>
     * <li>keyedScalars</li>
     * </ul>
     *
     * @param clear Whether the retrieved snapshots should be cleared.
     * @param response Used to return the async response.
     */
    public void getSnapshots(
          final boolean clear,
          final @NonNull GeckoResponse<GeckoBundle> response) {
        final GeckoBundle msg = new GeckoBundle(1);
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
