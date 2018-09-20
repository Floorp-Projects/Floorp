/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.telemetry.measurement;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.telemetry.config.TelemetryConfiguration;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.spy;

@RunWith(RobolectricTestRunner.class)
public class SessionDurationMeasurementTest {
    @Test
    public void testDefaultIsZero() {
        final TelemetryConfiguration configuration = new TelemetryConfiguration(RuntimeEnvironment.application);
        final SessionDurationMeasurement measurement = new SessionDurationMeasurement(configuration);

        final Object value = measurement.flush();
        assertNotNull(value);
        assertTrue(value instanceof Long);

        final long duration = (Long) value;
        assertEquals(0, duration);

        assertEquals(0, (long) measurement.flush());
    }

    @Test
    public void testRecordingSessionStartAndEnd() {
        final TelemetryConfiguration configuration = new TelemetryConfiguration(RuntimeEnvironment.application);

        final SessionDurationMeasurement measurement = spy(new SessionDurationMeasurement(configuration));
        doReturn(1337000000L).doReturn(5337000000L).when(measurement).getSystemTimeNano();

        assertEquals(0, (long) measurement.flush());

        measurement.recordSessionStart();

        // No duration is reported as long as the session is still active
        assertEquals(0, (long) measurement.flush());

        measurement.recordSessionEnd();

        // 4 seconds session
        assertEquals(4, (long) measurement.flush());

        // No new session was started so we should report 0 again
        assertEquals(0, (long) measurement.flush());
    }

    @Test
    public void testMultipleSessionsAreSummedUp() {
        final TelemetryConfiguration configuration = new TelemetryConfiguration(RuntimeEnvironment.application);

        final SessionDurationMeasurement measurement = spy(new SessionDurationMeasurement(configuration));
        doReturn(1337000000L) // Session start
                .doReturn(5337000000L) // Session end, duration: 4 seconds
                .doReturn(8337000000L) // Session start, 3 seconds after last session
                .doReturn(10337000000L) // Session end, duration: 2 seconds
                .when(measurement).getSystemTimeNano();

        assertEquals(0, (long) measurement.flush());

        measurement.recordSessionStart();
        measurement.recordSessionEnd(); // 4 seconds

        measurement.recordSessionStart();
        measurement.recordSessionEnd(); // 2 seconds

        // Total 6 seconds session
        assertEquals(6, (long) measurement.flush());

        // No new session was started so we should report 0 again
        assertEquals(0, (long) measurement.flush());
    }

    @Test(expected = IllegalStateException.class)
    public void testStartingAlreadyStartedSessionThrowsException() {
        final TelemetryConfiguration configuration = new TelemetryConfiguration(RuntimeEnvironment.application);
        final SessionDurationMeasurement measurement = new SessionDurationMeasurement(configuration);

        measurement.recordSessionStart();
        measurement.recordSessionStart();
    }

    @Test
    public void testStoppingAlreadyStoppedSessionReturnsFalse() {
        final TelemetryConfiguration configuration = new TelemetryConfiguration(RuntimeEnvironment.application);
        final SessionDurationMeasurement measurement = new SessionDurationMeasurement(configuration);

        measurement.recordSessionStart();

        assertTrue(measurement.recordSessionEnd());
        assertFalse(measurement.recordSessionEnd());
    }

    @Test
    public void testStoppingNeverStartedSessionReturnsFalse() {
        final TelemetryConfiguration configuration = new TelemetryConfiguration(RuntimeEnvironment.application);
        final SessionDurationMeasurement measurement = new SessionDurationMeasurement(configuration);

        assertFalse(measurement.recordSessionEnd());
    }
}
