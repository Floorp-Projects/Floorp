/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.telemetry.measurement;

import android.text.TextUtils;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.telemetry.config.TelemetryConfiguration;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;

import java.util.UUID;

import static org.junit.Assert.*;

@RunWith(RobolectricTestRunner.class)
public class ClientIdMeasurementTest {
    private static final String UUID_PATTERN = "[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}";

    @Test
    public void testClientIdIsAUUID() {
        final TelemetryConfiguration configuration = new TelemetryConfiguration(RuntimeEnvironment.application);

        final ClientIdMeasurement measurement = new ClientIdMeasurement(configuration);

        final Object value = measurement.flush();
        assertNotNull(value);
        assertTrue(value instanceof String);

        final String clientId = (String) value;
        assertFalse(TextUtils.isEmpty(clientId));
        assertTrue(clientId.matches(UUID_PATTERN));

        final UUID uuid = UUID.fromString(clientId); // Should throw if invalid
        assertEquals(4, uuid.version());
    }

    @Test
    public void testReturnsAlwaysTheSameClientId() {
        final TelemetryConfiguration configuration = new TelemetryConfiguration(RuntimeEnvironment.application);

        final ClientIdMeasurement measurement = new ClientIdMeasurement(configuration);
        final String clientId1 = (String) measurement.flush();
        final String clientId2 = (String) measurement.flush();

        assertEquals(clientId1, clientId2);
    }

    @Test
    public void testMultipleMeasurementsReturnSameClientId() {
        final TelemetryConfiguration configuration = new TelemetryConfiguration(RuntimeEnvironment.application);

        final String clientId1 = (String) new ClientIdMeasurement(configuration).flush();
        final String clientId2 = (String) new ClientIdMeasurement(configuration).flush();

        assertEquals(clientId1, clientId2);
    }
}