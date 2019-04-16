/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * vim: ts=4 sw=4 expandtab:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview;

import android.support.annotation.AnyThread;
import android.support.annotation.NonNull;

import org.json.JSONException;
import org.json.JSONObject;
import org.mozilla.gecko.EventDispatcher;
import org.mozilla.gecko.util.GeckoBundle;

/**
 * The telemetry API gives access to telemetry data of the Gecko runtime.
 */
public final class RuntimeTelemetry {
    private final EventDispatcher mEventDispatcher;

    /* package */ RuntimeTelemetry(final @NonNull GeckoRuntime runtime) {
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
     * @return A {@link GeckoResult} with the GeckoBundle snapshot results.
     */
    @AnyThread
    public @NonNull GeckoResult<JSONObject> getSnapshots(final boolean clear) {
        final GeckoBundle msg = new GeckoBundle(1);
        msg.putBoolean("clear", clear);

        final CallbackResult<JSONObject> result = new CallbackResult<JSONObject>() {
            @Override
            public void sendSuccess(final Object value) {
                try {
                    complete(((GeckoBundle) value).toJSONObject());
                } catch (JSONException ex) {
                    completeExceptionally(ex);
                }
            }
        };

        mEventDispatcher.dispatch("GeckoView:TelemetrySnapshots", msg, result);

        return result;
    }
}
