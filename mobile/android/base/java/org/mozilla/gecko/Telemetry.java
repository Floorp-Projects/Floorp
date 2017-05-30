/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.annotation.RobocopTarget;
import org.mozilla.gecko.annotation.WrapForJNI;
import org.mozilla.gecko.TelemetryContract.Event;
import org.mozilla.gecko.TelemetryContract.Method;
import org.mozilla.gecko.TelemetryContract.Reason;
import org.mozilla.gecko.TelemetryContract.Session;
import org.mozilla.gecko.mma.MmaDelegate;

import android.os.SystemClock;
import android.util.Log;

import static org.mozilla.gecko.mma.MmaDelegate.INTERACT_WITH_SEARCH_URL_AREA;
import static org.mozilla.gecko.mma.MmaDelegate.LOAD_BOOKMARK;
import static org.mozilla.gecko.mma.MmaDelegate.SAVE_BOOKMARK;
import static org.mozilla.gecko.mma.MmaDelegate.SAVE_PASSWORD;
import static org.mozilla.gecko.mma.MmaDelegate.WHEN_USER_TAKE_A_SCREENSHOT;


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

    @WrapForJNI(stubName = "AddHistogram", dispatchTo = "gecko")
    private static native void nativeAddHistogram(String name, int value);
    @WrapForJNI(stubName = "AddKeyedHistogram", dispatchTo = "gecko")
    private static native void nativeAddKeyedHistogram(String name, String key, int value);
    @WrapForJNI(stubName = "StartUISession", dispatchTo = "gecko")
    private static native void nativeStartUiSession(String name, long timestamp);
    @WrapForJNI(stubName = "StopUISession", dispatchTo = "gecko")
    private static native void nativeStopUiSession(String name, String reason, long timestamp);
    @WrapForJNI(stubName = "AddUIEvent", dispatchTo = "gecko")
    private static native void nativeAddUiEvent(String action, String method,
                                                long timestamp, String extras);

    public static long uptime() {
        return SystemClock.uptimeMillis();
    }

    public static long realtime() {
        return SystemClock.elapsedRealtime();
    }

    // Define new histograms in:
    // toolkit/components/telemetry/Histograms.json
    public static void addToHistogram(String name, int value) {
        if (GeckoThread.isRunning()) {
            nativeAddHistogram(name, value);
        } else {
            GeckoThread.queueNativeCall(Telemetry.class, "nativeAddHistogram",
                                        String.class, name, value);
        }
    }

    public static void addToKeyedHistogram(String name, String key, int value) {
        if (GeckoThread.isRunning()) {
            nativeAddKeyedHistogram(name, key, value);
        } else {
            GeckoThread.queueNativeCall(Telemetry.class, "nativeAddKeyedHistogram",
                                        String.class, name, String.class, key, value);
        }
    }

    public abstract static class Timer {
        private final long mStartTime;
        private final String mName;

        private volatile boolean mHasFinished;
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

            addToHistogram(mName, (int) (elapsed));
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

    public static void startUISession(final Session session, final String sessionNameSuffix) {
        final String sessionName = getSessionName(session, sessionNameSuffix);

        Log.d(LOGTAG, "StartUISession: " + sessionName);
        if (GeckoThread.isRunning()) {
            nativeStartUiSession(sessionName, realtime());
        } else {
            GeckoThread.queueNativeCall(Telemetry.class, "nativeStartUiSession",
                                        String.class, sessionName, realtime());
        }
    }

    public static void startUISession(final Session session) {
        startUISession(session, null);
    }

    public static void stopUISession(final Session session, final String sessionNameSuffix,
            final Reason reason) {
        final String sessionName = getSessionName(session, sessionNameSuffix);

        Log.d(LOGTAG, "StopUISession: " + sessionName + ", reason=" + reason);
        if (GeckoThread.isRunning()) {
            nativeStopUiSession(sessionName, reason.toString(), realtime());
        } else {
            GeckoThread.queueNativeCall(Telemetry.class, "nativeStopUiSession",
                                        String.class, sessionName,
                                        String.class, reason.toString(), realtime());
        }
    }

    public static void stopUISession(final Session session, final Reason reason) {
        stopUISession(session, null, reason);
    }

    public static void stopUISession(final Session session, final String sessionNameSuffix) {
        stopUISession(session, sessionNameSuffix, Reason.NONE);
    }

    public static void stopUISession(final Session session) {
        stopUISession(session, null, Reason.NONE);
    }

    private static String getSessionName(final Session session, final String sessionNameSuffix) {
        if (sessionNameSuffix != null) {
            return session.toString() + ":" + sessionNameSuffix;
        } else {
            return session.toString();
        }
    }

    /**
     * @param method A non-null method (if null is desired, consider using Method.NONE)
     */
    private static void sendUIEvent(final String eventName, final Method method,
            final long timestamp, final String extras) {
        if (method == null) {
            throw new IllegalArgumentException("Expected non-null method - use Method.NONE?");
        }

        if (!AppConstants.RELEASE_OR_BETA) {
            final String logString = "SendUIEvent: event = " + eventName + " method = " + method + " timestamp = " +
                    timestamp + " extras = " + extras;
            Log.d(LOGTAG, logString);
        }
        if (GeckoThread.isRunning()) {
            nativeAddUiEvent(eventName, method.toString(), timestamp, extras);
        } else {
            GeckoThread.queueNativeCall(Telemetry.class, "nativeAddUiEvent",
                                        String.class, eventName, String.class, method.toString(),
                                        timestamp, String.class, extras);
        }
        mappingMmaTracking(eventName, method, extras);
    }

    private static void mappingMmaTracking(String eventName, Method method, String extras) {
        if (eventName == null || method == null || extras == null) {
            return;
        }
        if (eventName.equalsIgnoreCase(Event.SAVE.toString()) && method == Method.MENU && extras.equals("bookmark")) {
            MmaDelegate.track(SAVE_BOOKMARK);
        } else if (eventName.equalsIgnoreCase(Event.LOAD_URL.toString()) && method == Method.LIST_ITEM && extras.equals("bookmarks")) {
            MmaDelegate.track(LOAD_BOOKMARK);
        } else if (eventName.equalsIgnoreCase(Event.SHOW.toString()) && method == Method.ACTIONBAR && extras.equals("urlbar-url")) {
            MmaDelegate.track(INTERACT_WITH_SEARCH_URL_AREA);
        } else if (eventName.equalsIgnoreCase(Event.SHARE.toString()) && method == Method.BUTTON && extras.equals("screenshot")) {
            MmaDelegate.track(WHEN_USER_TAKE_A_SCREENSHOT);
        } else if (eventName.equalsIgnoreCase(Event.ACTION.toString()) && method == Method.DOORHANGER && extras.equals("login-positive")) {
            MmaDelegate.track(SAVE_PASSWORD);
        }
    }

    public static void sendUIEvent(final Event event, final Method method, final long timestamp,
            final String extras) {
        sendUIEvent(event.toString(), method, timestamp, extras);
    }

    public static void sendUIEvent(final Event event, final Method method, final long timestamp) {
        sendUIEvent(event, method, timestamp, null);
    }

    public static void sendUIEvent(final Event event, final Method method, final String extras) {
        sendUIEvent(event, method, realtime(), extras);
    }

    public static void sendUIEvent(final Event event, final Method method) {
        sendUIEvent(event, method, realtime(), null);
    }

    public static void sendUIEvent(final Event event) {
        sendUIEvent(event, Method.NONE, realtime(), null);
    }

    /**
     * Sends a UIEvent with the given status appended to the event name.
     *
     * This method is a slight bend of the Telemetry framework so chances
     * are that you don't want to use this: please think really hard before you do.
     *
     * Intended for use with data policy notifications.
     */
    public static void sendUIEvent(final Event event, final boolean eventStatus) {
        final String eventName = event + ":" + eventStatus;
        sendUIEvent(eventName, Method.NONE, realtime(), null);
    }
}
