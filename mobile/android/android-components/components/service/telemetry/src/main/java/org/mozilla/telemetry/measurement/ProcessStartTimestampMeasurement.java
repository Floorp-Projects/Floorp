package org.mozilla.telemetry.measurement;

import org.mozilla.telemetry.Telemetry;
import org.mozilla.telemetry.config.TelemetryConfiguration;

public class ProcessStartTimestampMeasurement extends TelemetryMeasurement {
    private static final String FIELD_NAME = "processStartTimestamp";

    private final long processLoadTimestampMillis;

    public ProcessStartTimestampMeasurement(final TelemetryConfiguration configuration) {
        super(FIELD_NAME);

        // Ideally, we'd get the process start time, but considering Telemetry is
        // expected to be loaded early in the Activity lifecycle, this is good enough.
        processLoadTimestampMillis = configuration.getClassLoadTimestampMillis();
    }

    @Override
    public Object flush() {
        return processLoadTimestampMillis;
    }
}
