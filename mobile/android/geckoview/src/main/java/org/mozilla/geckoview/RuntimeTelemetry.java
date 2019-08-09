/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * vim: ts=4 sw=4 expandtab:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview;

import android.support.annotation.AnyThread;
import android.support.annotation.NonNull;

import java.util.Arrays;

import org.json.JSONException;
import org.json.JSONObject;

import org.mozilla.gecko.annotation.WrapForJNI;
import org.mozilla.gecko.EventDispatcher;
import org.mozilla.gecko.util.GeckoBundle;
import org.mozilla.gecko.mozglue.JNIObject;
import org.mozilla.gecko.GeckoThread;

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

    /**
     * The runtime telemetry metric object.
     */
    public static class Metric {
        /**
         * The runtime metric name.
         */
        public final @NonNull String name;

        /**
         * The metric values.
         */
        public final @NonNull long[] values;

        /* package */  Metric(final String name, final long[] values) {
            this.name = name;
            this.values = values;
        }

        @Override
        public String toString() {
            return "name: " + name + ", values: " + Arrays.toString(values);
        }

        protected Metric() {
            this.name = null;
            this.values = null;
        }
    }

    /**
     * The runtime telemetry delegate.
     * Implement this if you want to receive runtime (Gecko) telemetry and
     * attach it via {@link GeckoRuntimeSettings.Builder#telemetryDelegate}.
     */
    public interface Delegate {
        /**
         * A runtime telemetry metric has been received.
         *
         * @param metric The runtime metric details.
         */
        @AnyThread
        default void onTelemetryReceived(final @NonNull Metric metric) {}
    }

    // The proxy connects to telemetry core and forwards telemetry events
    // to the attached delegate.
    /* package */ final static class Proxy extends JNIObject  {
        private final Delegate mDelegate;

        public Proxy(final @NonNull Delegate delegate) {
            mDelegate = delegate;
            // We might want to remove implicit attaching in future.
            attach();
        }

        // Attach to current runtime.
        // We might have different mechanics of attaching to specific runtimes
        // in future, for which case we should split the delegate assignment in
        // the setup phase from the attaching.
        public void attach() {
            if (GeckoThread.isRunning()) {
                registerDelegateProxy(this);
            } else {
                GeckoThread.queueNativeCall(
                    Proxy.class, "registerDelegateProxy",
                    Proxy.class, this);
            }
        }

        public @NonNull Delegate getDelegate() {
            return mDelegate;
        }

        @WrapForJNI(dispatchTo = "gecko")
        private static native void registerDelegateProxy(Proxy proxy);

        @WrapForJNI(calledFrom = "gecko")
        /* package */ void dispatchTelemetry(
                final String name, final long[] values) {
            if (mDelegate == null) {
                // TODO throw?
                return;
            }
            mDelegate.onTelemetryReceived(new Metric(name, values));
        }

        @Override // JNIObject
        protected void disposeNative() {
            // We don't hold native references.
            throw new UnsupportedOperationException();
        }
    }
}
