/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.annotation.WrapForJNI;

import android.os.SystemClock;
import android.util.Log;

/**
 * All telemetry times are relative to one of two clocks:
 *
 * * Real time since the device was booted, including deep sleep. Use this
 *   as a substitute for wall clock.
 * * Uptime since the device was booted, excluding deep sleep. Use this to
 *   avoid timing a user activity when their phone is in their pocket!
 *
 * The majority of methods in this class are defined in terms of real time.
 */
public class TelemetryUtils {
    private static final String LOGTAG = "TelemetryUtils";

    @WrapForJNI(stubName = "AddHistogram", dispatchTo = "gecko")
    private static native void nativeAddHistogram(String name, int value);

    public static long uptime() {
        return SystemClock.uptimeMillis();
    }

    public static long realtime() {
        return SystemClock.elapsedRealtime();
    }

    // Define new histograms in:
    // toolkit/components/telemetry/Histograms.json
    public static void addToHistogram(final String name, final int value) {
        if (GeckoThread.isRunning()) {
            nativeAddHistogram(name, value);
        } else {
            GeckoThread.queueNativeCall(TelemetryUtils.class, "nativeAddHistogram",
                                        String.class, name, value);
        }
    }

    public abstract static class Timer {
        private final long mStartTime;
        private final String mName;

        private volatile boolean mHasFinished;
        private volatile long mElapsed = -1;

        protected abstract long now();

        public Timer(final String name) {
            mName = name;
            mStartTime = now();
        }

        public void cancel() {
            mHasFinished = true;
        }

        public long getElapsed() {
            return mElapsed;
        }

        public void stop() {
            // Only the first stop counts.
            if (mHasFinished) {
                return;
            }

            mHasFinished = true;

            final long elapsed = now() - mStartTime;
            if (elapsed < 0) {
                Log.e(LOGTAG, "Current time less than start time -- clock shenanigans?");
                return;
            }

            mElapsed = elapsed;
            if (elapsed > Integer.MAX_VALUE) {
                Log.e(LOGTAG, "Duration of " + elapsed + "ms is too great to add to histogram.");
                return;
            }

            addToHistogram(mName, (int) (elapsed));
        }
    }

    public static class UptimeTimer extends Timer {
        public UptimeTimer(final String name) {
            super(name);
        }

        @Override
        protected long now() {
            return TelemetryUtils.uptime();
        }
    }
}
