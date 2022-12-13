/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.telemetry.measurement;

import org.json.JSONObject;

public class MetricsMeasurement extends TelemetryMeasurement {
    private static final String FIELD_NAME = "metrics";
    private JSONObject snapshot;

    public MetricsMeasurement(JSONObject snapshot) {
        super(FIELD_NAME);
        this.snapshot = snapshot;
    }

    @Override
    public Object flush() {
        return snapshot;
    }
}
