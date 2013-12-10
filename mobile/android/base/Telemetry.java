/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.mozglue.RobocopTarget;

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
@RobocopTarget
public class Telemetry {
    private static final String LOGTAG = "Telemetry";

    public static long uptime() {
        return SystemClock.uptimeMillis();
    }

    public static long realtime() {
        return SystemClock.elapsedRealtime();
    }

    // Define new histograms in:
    // toolkit/components/telemetry/Histograms.json
    public static void HistogramAdd(String name, int value) {
        GeckoEvent event = GeckoEvent.createTelemetryHistogramAddEvent(name, value);
        GeckoAppShell.sendEventToGecko(event);
    }

    public abstract static class Timer {
        private final long mStartTime;
        private final String mName;

        private volatile boolean mHasFinished = false;
        private volatile long mElapsed = -1;

        protected abstract long now();

        public Timer(String name) {
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

            HistogramAdd(mName, (int)(elapsed));
        }
    }

    public static class RealtimeTimer extends Timer {
        public RealtimeTimer(String name) {
            super(name);
        }

        @Override
        protected long now() {
            return Telemetry.realtime();
        }
    }

    public static class UptimeTimer extends Timer {
        public UptimeTimer(String name) {
            super(name);
        }

        @Override
        protected long now() {
            return Telemetry.uptime();
        }
    }

    public static void startUISession(String sessionName) {
        GeckoEvent event = GeckoEvent.createTelemetryUISessionStartEvent(sessionName, realtime());
        GeckoAppShell.sendEventToGecko(event);
    }

    public static void stopUISession(String sessionName, String reason) {
        GeckoEvent event = GeckoEvent.createTelemetryUISessionStopEvent(sessionName, reason, realtime());
        GeckoAppShell.sendEventToGecko(event);
    }

    public static void sendUIEvent(String action, String method, long timestamp, String extras) {
        GeckoEvent event = GeckoEvent.createTelemetryUIEvent(action, method, timestamp, extras);
        GeckoAppShell.sendEventToGecko(event);
    }

    public static void sendUIEvent(String action, String method, long timestamp) {
        sendUIEvent(action, method, timestamp, null);
    }

    public static void sendUIEvent(String action, String method, String extras) {
        sendUIEvent(action, method, realtime(), extras);
    }

    public static void sendUIEvent(String action, String method) {
        sendUIEvent(action, method, realtime(), null);
    }

    public static void sendUIEvent(String action) {
        sendUIEvent(action, null, realtime(), null);
    }
}
