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
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.spy;

@RunWith(RobolectricTestRunner.class)
public class FirstRunProfileDateMeasurementTest {
    @Test
    public void testDefaultValue() {
        final TelemetryConfiguration configuration = new TelemetryConfiguration(RuntimeEnvironment.application);
        final FirstRunProfileDateMeasurement measurement = new FirstRunProfileDateMeasurement(configuration);

        final Object value = measurement.flush();
        assertNotNull(value);
        assertTrue(value instanceof Long);

        final long profileCreationInDays = (long) value; // in days since UNIX epoch.
        assertTrue(profileCreationInDays > 17261); // This is the timestamp of the day before the test was written
        assertTrue(profileCreationInDays < System.currentTimeMillis()); // Days < Milliseconds
    }

    @Test
    public void testCalculation() {
        final TelemetryConfiguration configuration = new TelemetryConfiguration(RuntimeEnvironment.application);
        final FirstRunProfileDateMeasurement measurement = spy(new FirstRunProfileDateMeasurement(configuration));

        // April 6, 2017
        doReturn(1491487779305L).when(measurement).now();

        // Re-run because the first time it was called from the constructor and we couldn't mock now() before
        // the object is actually constructed.
        configuration.getSharedPreferences().edit().clear().apply();
        measurement.ensureValueExists();

        // January 1, 1970 -> April 6, 2017 -> 17262 days
        assertEquals(17262L, measurement.flush());
    }

    @Test
    public void testFallback() {
        final TelemetryConfiguration configuration = new TelemetryConfiguration(RuntimeEnvironment.application);
        final FirstRunProfileDateMeasurement measurement = spy(new FirstRunProfileDateMeasurement(configuration));

        // Remove stored first run date
        configuration.getSharedPreferences()
                .edit()
                .clear()
                .apply();

        final Object value = measurement.flush();
        assertNotNull(value);
        assertTrue(value instanceof Long);

        // We should still return a date that makes sense (like as if the profile was created right now)
        final long profileCreationInDays = (long) value;
        assertTrue(profileCreationInDays > 17261);
        assertTrue(profileCreationInDays < System.currentTimeMillis());

    }
}