/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.telemetry.pingbuilders;

import android.os.Bundle;

import org.json.simple.JSONArray;
import org.mozilla.gecko.sync.ExtendedJSONObject;
import org.mozilla.gecko.sync.telemetry.TelemetryContract;
import org.mozilla.gecko.telemetry.TelemetryLocalPing;

public class TelemetrySyncEventPingBuilder extends TelemetryLocalPingBuilder {

    @SuppressWarnings("unchecked")
    public TelemetrySyncEventPingBuilder fromEventTelemetry(Bundle data) {
        final long timestamp = data.getLong(TelemetryContract.KEY_EVENT_TIMESTAMP, -1L);
        final String category = data.getString(TelemetryContract.KEY_EVENT_CATEGORY);
        final String object = data.getString(TelemetryContract.KEY_EVENT_OBJECT);
        final String method = data.getString(TelemetryContract.KEY_EVENT_METHOD);
        final String value = data.getString(TelemetryContract.KEY_EVENT_VALUE);
        final Bundle extra = data.getBundle(TelemetryContract.KEY_EVENT_EXTRA);

        if (timestamp == -1L || category == null || object == null || method == null) {
            throw new IllegalStateException("Bundle should be well formed.");
        }

        final JSONArray event = new JSONArray();
        // Events are serialized as arrays when sending the sync ping. The order of the following
        // statements SHOULD NOT be changed unless the telemetry server specification changes.
        event.add(timestamp);
        event.add(category);
        event.add(method);
        event.add(object);
        if (value != null || extra != null) {
            event.add(value);
            if (extra != null) {
                final ExtendedJSONObject extraJSON = new ExtendedJSONObject();
                for (final String k : extra.keySet()) {
                    extraJSON.put(k, extra.getString(k));
                }
                event.add(extraJSON);
            }
        }
        /**
         * Note: {@link org.mozilla.gecko.telemetry.TelemetryOutgoingPing#getPayload()}
         * returns ExtendedJSONObject. Wrap our JSONArray into the payload JSON object.
         */
        payload.put("event", event);
        return this;
    }

    @Override
    public TelemetryLocalPing build() {
        return new TelemetryLocalPing(payload, docID);
    }
}
