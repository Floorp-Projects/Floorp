/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.telemetry.ping;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.telemetry.config.TelemetryConfiguration;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;

import java.util.Map;
import java.util.UUID;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

@RunWith(RobolectricTestRunner.class)
public class TelemetryEventPingBuilderTest {
    @Test
    public void testBuildingEmptyPing() {
        final TelemetryConfiguration configuration = new TelemetryConfiguration(RuntimeEnvironment.application);
        final TelemetryEventPingBuilder builder = new TelemetryEventPingBuilder(configuration);

        final TelemetryPing ping = builder.build();

        assertUUID(ping.getDocumentId());
        assertEquals("focus-event", ping.getType());

        final Map<String, Object> results = ping.getMeasurementResults();
        assertNotNull(results);

        assertTrue(results.containsKey("seq"));
        assertEquals(1L, results.get("seq"));

        assertTrue(results.containsKey("locale"));
        assertEquals("en-US", results.get("locale"));

        assertTrue(results.containsKey("os"));
        assertEquals("Android", results.get("os"));

        assertTrue(results.containsKey("osversion"));
        assertEquals("27", results.get("osversion"));
    }

    @Test
    public void testSequenceNumber() {
        final TelemetryConfiguration configuration = new TelemetryConfiguration(RuntimeEnvironment.application);
        final TelemetryEventPingBuilder builder = new TelemetryEventPingBuilder(configuration);

        {
            final TelemetryPing ping1 = builder.build();

            final Map<String, Object> results = ping1.getMeasurementResults();
            assertNotNull(results);

            assertTrue(results.containsKey("seq"));
            assertEquals(1L, results.get("seq"));
        }

        {
            final TelemetryPing ping2 = builder.build();

            final Map<String, Object> results2 = ping2.getMeasurementResults();
            assertNotNull(results2);

            assertTrue(results2.containsKey("seq"));
            assertEquals(2L, results2.get("seq"));
        }
    }

    private void assertUUID(String uuid) {
        //noinspection ResultOfMethodCallIgnored
        UUID.fromString(uuid); // Should throw IllegalArgumentException if parameter does not conform
    }
}