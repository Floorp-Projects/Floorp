/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.telemetry.net;

import android.util.Log;

import org.json.JSONException;
import org.json.JSONObject;
import org.mozilla.telemetry.config.TelemetryConfiguration;

/**
 * This client just prints pings to logcat instead of uploading them. Therefore this client is only
 * useful for debugging purposes.
 */
public class DebugLogClient implements TelemetryClient {
    private final String tag;

    public DebugLogClient(String tag) {
        this.tag = tag;
    }

    @Override
    public boolean uploadPing(TelemetryConfiguration configuration, String path, String serializedPing) {
        Log.d(tag, "---PING--- " + path);

        try {
            JSONObject object = new JSONObject(serializedPing);
            Log.d(tag, object.toString(2));
        } catch (JSONException e) {
            Log.d(tag, "Corrupt JSON", e);
        }

        return true;
    }
}
