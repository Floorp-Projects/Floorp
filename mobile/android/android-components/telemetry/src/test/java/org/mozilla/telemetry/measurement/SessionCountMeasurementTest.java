/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.telemetry.measurement;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.telemetry.config.TelemetryConfiguration;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;

import static org.junit.Assert.*;

@RunWith(RobolectricTestRunner.class)
public class SessionCountMeasurementTest {
    @Test
    public void testByDefaultSessionCountStaysAtZero() {
        final TelemetryConfiguration configuration = new TelemetryConfiguration(RuntimeEnvironment.application);
        final SessionCountMeasurement measurement = new SessionCountMeasurement(configuration);

        {
            final Object value = measurement.flush();
            assertNotNull(value);
            assertTrue(value instanceof Long);

            final long count = (Long) value;
            assertEquals(0, count);
        }
        {
            final Object value = measurement.flush();
            assertNotNull(value);
            assertTrue(value instanceof Long);

            final long count = (Long) value;
            assertEquals(0, count);
        }
    }

    @Test
    public void testCountingSession() {
        final TelemetryConfiguration configuration = new TelemetryConfiguration(RuntimeEnvironment.application);
        final SessionCountMeasurement measurement = new SessionCountMeasurement(configuration);

        assertEquals(0, (long) measurement.flush());

        measurement.countSession();

        assertEquals(1, (long) measurement.flush());

        // Counter is reset after every flush
        assertEquals(0, (long) measurement.flush());

        measurement.countSession();
        measurement.countSession();
        measurement.countSession();

        assertEquals(3, (long) measurement.flush());
        assertEquals(0, (long) measurement.flush());
    }
}
