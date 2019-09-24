/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.telemetry.measurement;

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
