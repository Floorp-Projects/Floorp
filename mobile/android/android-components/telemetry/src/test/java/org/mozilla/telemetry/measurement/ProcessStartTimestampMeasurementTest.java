/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.telemetry.measurement;

import org.junit.Test;
import org.mozilla.telemetry.config.TelemetryConfiguration;

import static org.junit.Assert.*;
import static org.mockito.Mockito.*;

public class ProcessStartTimestampMeasurementTest {
    @Test
    public void testTimestampComesFromTelemetryConfiguration() throws Exception {
        final long expectedTimestamp = 1357389201;
        final TelemetryConfiguration mockTelemetryConfiguration = mock(TelemetryConfiguration.class);
        when(mockTelemetryConfiguration.getClassLoadTimestampMillis()).thenReturn(expectedTimestamp);
        assertEquals(expectedTimestamp, new ProcessStartTimestampMeasurement(mockTelemetryConfiguration).flush());
    }
}
