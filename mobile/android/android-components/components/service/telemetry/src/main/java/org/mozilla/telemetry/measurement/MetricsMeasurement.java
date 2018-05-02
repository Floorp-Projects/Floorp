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
