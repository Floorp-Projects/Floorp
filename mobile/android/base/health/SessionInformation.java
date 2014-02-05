/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.health;

import android.content.SharedPreferences;
import android.util.Log;

import org.mozilla.gecko.GeckoApp;

import org.json.JSONException;
import org.json.JSONObject;

public class SessionInformation {
    private static final String LOG_TAG = "GeckoSessInfo";

    public static final String PREFS_SESSION_START = "sessionStart";

    public final long wallStartTime;    // System wall clock.
    public final long realStartTime;    // Realtime clock.

    private final boolean wasOOM;
    private final boolean wasStopped;

    private volatile long timedGeckoStartup = -1;
    private volatile long timedJavaStartup = -1;

    // Current sessions don't (right now) care about wasOOM/wasStopped.
    // Eventually we might want to lift that logic out of GeckoApp.
    public SessionInformation(long wallTime, long realTime) {
        this(wallTime, realTime, false, false);
    }

    // Previous sessions do...
    public SessionInformation(long wallTime, long realTime, boolean wasOOM, boolean wasStopped) {
        this.wallStartTime = wallTime;
        this.realStartTime = realTime;
        this.wasOOM = wasOOM;
        this.wasStopped = wasStopped;
    }

    /**
     * Initialize a new SessionInformation instance from the supplied prefs object.
     *
     * This includes retrieving OOM/crash data, as well as timings.
     *
     * If no wallStartTime was found, that implies that the previous
     * session was correctly recorded, and an object with a zero
     * wallStartTime is returned.
     */
    public static SessionInformation fromSharedPrefs(SharedPreferences prefs) {
        boolean wasOOM = prefs.getBoolean(GeckoApp.PREFS_OOM_EXCEPTION, false);
        boolean wasStopped = prefs.getBoolean(GeckoApp.PREFS_WAS_STOPPED, true);
        long wallStartTime = prefs.getLong(PREFS_SESSION_START, 0L);
        long realStartTime = 0L;
        Log.d(LOG_TAG, "Building SessionInformation from prefs: " +
                       wallStartTime + ", " + realStartTime + ", " +
                       wasStopped + ", " + wasOOM);
        return new SessionInformation(wallStartTime, realStartTime, wasOOM, wasStopped);
    }

    /**
     * Initialize a new SessionInformation instance to 'split' the current
     * session.
     */
    public static SessionInformation forRuntimeTransition() {
        final boolean wasOOM = false;
        final boolean wasStopped = true;
        final long wallStartTime = System.currentTimeMillis();
        final long realStartTime = android.os.SystemClock.elapsedRealtime();
        Log.v(LOG_TAG, "Recording runtime session transition: " +
                       wallStartTime + ", " + realStartTime);
        return new SessionInformation(wallStartTime, realStartTime, wasOOM, wasStopped);
    }

    public boolean wasKilled() {
        return wasOOM || !wasStopped;
    }

    /**
     * Record the beginning of this session to SharedPreferences by
     * recording our start time. If a session was already recorded, it is
     * overwritten (there can only be one running session at a time). Does
     * not commit the editor.
     */
    public void recordBegin(SharedPreferences.Editor editor) {
        Log.d(LOG_TAG, "Recording start of session: " + this.wallStartTime);
        editor.putLong(PREFS_SESSION_START, this.wallStartTime);
    }

    /**
     * Record the completion of this session to SharedPreferences by
     * deleting our start time. Does not commit the editor.
     */
    public void recordCompletion(SharedPreferences.Editor editor) {
        Log.d(LOG_TAG, "Recording session done: " + this.wallStartTime);
        editor.remove(PREFS_SESSION_START);
    }

    /**
     * Return the JSON that we'll put in the DB for this session.
     */
    public JSONObject getCompletionJSON(String reason, long realEndTime) throws JSONException {
        long durationSecs = (realEndTime - this.realStartTime) / 1000;
        JSONObject out = new JSONObject();
        out.put("r", reason);
        out.put("d", durationSecs);
        if (this.timedGeckoStartup > 0) {
            out.put("sg", this.timedGeckoStartup);
        }
        if (this.timedJavaStartup > 0) {
            out.put("sj", this.timedJavaStartup);
        }
        return out;
    }

    public JSONObject getCrashedJSON() throws JSONException {
        JSONObject out = new JSONObject();
        // We use ints here instead of booleans, because we're packing
        // stuff into JSON, and saving bytes in the DB is a worthwhile
        // goal.
        out.put("oom", this.wasOOM ? 1 : 0);
        out.put("stopped", this.wasStopped ? 1 : 0);
        out.put("r", "A");
        return out;
    }

    public void setTimedGeckoStartup(final long duration) {
        timedGeckoStartup = duration;
    }

    public void setTimedJavaStartup(final long duration) {
        timedJavaStartup = duration;
    }
}
