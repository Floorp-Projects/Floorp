/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.health;

import android.content.ContentProviderClient;
import android.content.Context;
import android.util.Log;

import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.GeckoEvent;
import org.mozilla.gecko.GeckoProfile;

import org.mozilla.gecko.background.healthreport.EnvironmentBuilder;
import org.mozilla.gecko.background.healthreport.HealthReportConstants;
import org.mozilla.gecko.background.healthreport.HealthReportDatabaseStorage;
import org.mozilla.gecko.background.healthreport.HealthReportGenerator;

import org.mozilla.gecko.util.GeckoEventListener;
import org.mozilla.gecko.util.ThreadUtils;

import org.json.JSONException;
import org.json.JSONObject;

/**
 * BrowserHealthReporter is the browser's interface to the Firefox Health
 * Report report generator.
 *
 * Each instance registers Gecko event listeners, so keep a single instance
 * around for the life of the browser. Java callers should use this globally
 * available singleton.
 */
public class BrowserHealthReporter implements GeckoEventListener {
    private static final String LOGTAG = "GeckoHealthRep";

    public static final String EVENT_REQUEST  = "HealthReport:Request";
    public static final String EVENT_RESPONSE = "HealthReport:Response";

    protected final Context context;

    public BrowserHealthReporter() {
        GeckoAppShell.registerEventListener(EVENT_REQUEST, this);

        context = GeckoAppShell.getContext();
        if (context == null) {
            throw new IllegalStateException("Null Gecko context");
        }
    }

    public void uninit() {
        GeckoAppShell.unregisterEventListener(EVENT_REQUEST, this);
    }

    /**
     * Generate a new Health Report.
     *
     * This method performs IO, so call it from a background thread.
     *
     * @param since timestamp of first day to report (milliseconds since epoch).
     * @param lastPingTime timestamp when last health report was uploaded
     *                     (milliseconds since epoch).
     * @param profilePath path of the profile to generate report for.
     * @throws JSONException if JSON generation fails.
     * @throws IllegalStateException if the environment does not allow to generate a report.
     * @return non-null report.
     */
    public JSONObject generateReport(long since, long lastPingTime, String profilePath) throws JSONException {
        // We abuse the life-cycle of an Android ContentProvider slightly by holding
        // onto a ContentProviderClient while we generate a payload. This keeps
        // our database storage alive, while also allowing us to share a database
        // connection with BrowserHealthRecorder and the uploader.
        // The ContentProvider owns all underlying Storage instances, so we don't
        // need to explicitly close them.
        ContentProviderClient client = EnvironmentBuilder.getContentProviderClient(context);
        if (client == null) {
            throw new IllegalStateException("Could not fetch Health Report content provider.");
        }

        try {
            // Storage instance is owned by HealthReportProvider, so we don't need
            // to close it.
            HealthReportDatabaseStorage storage = EnvironmentBuilder.getStorage(client, profilePath);
            if (storage == null) {
                throw new IllegalStateException("No storage in Health Reporter.");
            }

            HealthReportGenerator generator = new HealthReportGenerator(storage);
            JSONObject report = generator.generateDocument(since, lastPingTime, profilePath);
            if (report == null) {
                throw new IllegalStateException("Not enough profile information to generate report.");
            }
            return report;
        } finally {
            client.release();
        }
    }

    /**
     * Get last time a health report was successfully uploaded.
     *
     * This is read from shared preferences, so call it from a background
     * thread.  Bug 882182 tracks making this work with multiple profiles.
     *
     * @return milliseconds since the epoch, or 0 if never uploaded.
     */
    protected long getLastUploadLocalTime() {
        return context
            .getSharedPreferences(HealthReportConstants.PREFS_BRANCH, 0)
            .getLong(HealthReportConstants.PREF_LAST_UPLOAD_LOCAL_TIME, 0L);
    }

    /**
     * Generate a new Health Report for the current Gecko profile.
     *
     * This method performs IO, so call it from a background thread.
     *
     * @throws JSONException if JSON generation fails.
     * @throws IllegalStateException if the environment does not allow to generate a report.
     * @return non-null Health Report.
     */
    public JSONObject generateReport() throws JSONException {
        GeckoProfile profile = GeckoAppShell.getGeckoInterface().getProfile();
        String profilePath = profile.getDir().getAbsolutePath();

        long since = System.currentTimeMillis() - HealthReportConstants.MILLISECONDS_PER_SIX_MONTHS;
        long lastPingTime = Math.max(getLastUploadLocalTime(), HealthReportConstants.EARLIEST_LAST_PING);

        return generateReport(since, lastPingTime, profilePath);
    }

    @Override
    public void handleMessage(String event, JSONObject message) {
        try {
            ThreadUtils.postToBackgroundThread(new Runnable() {
                @Override
                public void run() {
                    JSONObject report = null;
                    try {
                        report = generateReport(); // non-null if it returns.
                    } catch (Exception e) {
                        Log.e(LOGTAG, "Generating report failed; responding with empty report.", e);
                        report = new JSONObject();
                    }

                    GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent(EVENT_RESPONSE, report.toString()));
                }
           });
        } catch (Exception e) {
            Log.e(LOGTAG, "Exception handling message \"" + event + "\":", e);
        }
    }
}

