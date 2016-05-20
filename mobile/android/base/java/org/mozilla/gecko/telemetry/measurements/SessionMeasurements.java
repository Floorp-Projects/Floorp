/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, you can obtain one at http://mozilla.org/MPL/2.0/.
 */

package org.mozilla.gecko.telemetry.measurements;

import android.content.Context;
import android.content.SharedPreferences;
import android.support.annotation.UiThread;
import android.support.annotation.VisibleForTesting;
import org.mozilla.gecko.GeckoSharedPrefs;

import java.util.concurrent.TimeUnit;

/**
 * A class to measure the number of user sessions & their durations. It was created for use with the
 * telemetry core ping. A session is the time between {@link #recordSessionStart()} and
 * {@link #recordSessionEnd(Context)}.
 *
 * This class is thread-safe, provided the thread annotations are followed. Under the hood, this class uses
 * SharedPreferences & because there is no atomic getAndSet operation, we synchronize access to it.
 */
public class SessionMeasurements {
    @VisibleForTesting static final String PREF_SESSION_COUNT = "measurements-session-count";
    @VisibleForTesting static final String PREF_SESSION_DURATION = "measurements-session-duration";

    private boolean sessionStarted = false;
    private long timeAtSessionStartNano = -1;

    @UiThread // we assume this will be called on the same thread as session end so we don't have to synchronize sessionStarted.
    public void recordSessionStart() {
        if (sessionStarted) {
            throw new IllegalStateException("Trying to start session but it is already started");
        }
        sessionStarted = true;
        timeAtSessionStartNano = getSystemTimeNano();
    }

    @UiThread // we assume this will be called on the same thread as session start so we don't have to synchronize sessionStarted.
    public void recordSessionEnd(final Context context) {
        if (!sessionStarted) {
            throw new IllegalStateException("Expected session to be started before session end is called");
        }
        sessionStarted = false;

        final long sessionElapsedSeconds = TimeUnit.NANOSECONDS.toSeconds(getSystemTimeNano() - timeAtSessionStartNano);
        final SharedPreferences sharedPrefs = getSharedPreferences(context);
        synchronized (this) {
            final int sessionCount = sharedPrefs.getInt(PREF_SESSION_COUNT, 0);
            final long totalElapsedSeconds = sharedPrefs.getLong(PREF_SESSION_DURATION, 0);
            sharedPrefs.edit()
                    .putInt(PREF_SESSION_COUNT, sessionCount + 1)
                    .putLong(PREF_SESSION_DURATION, totalElapsedSeconds + sessionElapsedSeconds)
                    .apply();
        }
    }

    /**
     * Gets the session measurements since the last time the measurements were last retrieved.
     */
    public synchronized SessionMeasurementsContainer getAndResetSessionMeasurements(final Context context) {
        final SharedPreferences sharedPrefs = getSharedPreferences(context);
        final int sessionCount = sharedPrefs.getInt(PREF_SESSION_COUNT, 0);
        final long totalElapsedSeconds = sharedPrefs.getLong(PREF_SESSION_DURATION, 0);
        sharedPrefs.edit()
                .putInt(PREF_SESSION_COUNT, 0)
                .putLong(PREF_SESSION_DURATION, 0)
                .apply();
        return new SessionMeasurementsContainer(sessionCount, totalElapsedSeconds);
    }

    @VisibleForTesting SharedPreferences getSharedPreferences(final Context context) {
        return GeckoSharedPrefs.forProfile(context);
    }

    /**
     * Returns (roughly) the system uptime in nanoseconds. A less coupled implementation would
     * take this value from the caller of recordSession*, however, we do this internally to ensure
     * the caller uses both a time system consistent between the start & end calls and uses the
     * appropriate time system (i.e. not wall time, which can change when the clock is changed).
     */
    @VisibleForTesting long getSystemTimeNano() { // TODO: necessary?
        return System.nanoTime();
    }

    public static final class SessionMeasurementsContainer {
        /** The number of sessions. */
        public final int sessionCount;
        /** The number of seconds elapsed in ALL sessions included in {@link #sessionCount}. */
        public final long elapsedSeconds;

        private SessionMeasurementsContainer(final int sessionCount, final long elapsedSeconds) {
            this.sessionCount = sessionCount;
            this.elapsedSeconds = elapsedSeconds;
        }
    }
}
