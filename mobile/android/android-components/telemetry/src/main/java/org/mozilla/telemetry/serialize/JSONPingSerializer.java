/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.telemetry.serialize;

import org.json.JSONException;
import org.json.JSONObject;
import org.mozilla.telemetry.ping.TelemetryPing;

import java.util.Map;

/**
 * TelemetryPingSerializer that uses the org.json library provided by the Android system.
 */
public class JSONPingSerializer implements TelemetryPingSerializer {
    @Override
    public String serialize(TelemetryPing ping) {
        try {
            final JSONObject object = new JSONObject();

            for (Map.Entry<String, Object> result : ping.getMeasurementResults().entrySet()) {
                object.put(result.getKey(), result.getValue());
            }

            return object.toString();
        } catch (JSONException e) {
            throw new AssertionError("Can't serialize ping", e);
        }
    }
}
