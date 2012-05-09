/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.os.SystemClock;
import android.util.Log;

import org.json.JSONException;
import org.json.JSONObject;

public class Telemetry {
    private static final String LOGTAG = "Telemetry";

    // Define new histograms in:
    // toolkit/components/telemetry/TelemetryHistograms.h
    public static void HistogramAdd(String name,
                                    int value) {
        try {
            JSONObject jsonData = new JSONObject();

            jsonData.put("name", name);
            jsonData.put("value", value);

            GeckoEvent event =
                GeckoEvent.createBroadcastEvent("Telemetry:Add", jsonData.toString());
            GeckoAppShell.sendEventToGecko(event);

            Log.v(LOGTAG, "Sending telemetry: " + jsonData.toString());
        } catch (JSONException e) {
            Log.e(LOGTAG, "JSON exception: ", e);
        }
    }

    public static class Timer {
        private long mStartTime;
        private String mName;

        public Timer(String name) {
            mName = name;
            mStartTime = SystemClock.uptimeMillis();
        }

        public void stop() {
            long elapsed = SystemClock.uptimeMillis() - mStartTime;
            if (elapsed < Integer.MAX_VALUE) {
                HistogramAdd(mName, (int)(elapsed));
            } else {
                Log.e(LOGTAG, "Duration of " + elapsed + " ms is too long.");
            }
        }
    }
}
