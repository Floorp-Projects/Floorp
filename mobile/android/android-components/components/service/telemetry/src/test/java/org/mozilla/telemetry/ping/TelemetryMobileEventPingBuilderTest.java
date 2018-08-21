/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.telemetry.ping;

import org.json.JSONObject;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.telemetry.config.TelemetryConfiguration;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;

import java.util.HashMap;
import java.util.Map;
import java.util.UUID;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

@RunWith(RobolectricTestRunner.class)
public class TelemetryMobileEventPingBuilderTest {
    @Test
    public void testBuildingEmptyPing() {
        final TelemetryConfiguration configuration = new TelemetryConfiguration(RuntimeEnvironment.application);
        final TelemetryMobileEventPingBuilder builder = new TelemetryMobileEventPingBuilder(configuration);

        final TelemetryPing ping = builder.build();

        assertUUID(ping.getDocumentId());
        assertEquals("mobile-event", ping.getType());

        final Map<String, Object> results = ping.getMeasurementResults();
        assertNotNull(results);

        assertTrue(results.containsKey("seq"));
        assertEquals(1L, results.get("seq"));

        assertTrue(results.containsKey("locale"));
        assertEquals("en-US", results.get("locale"));

        assertTrue(results.containsKey("os"));
        assertEquals("Android", results.get("os"));

        assertTrue(results.containsKey("osversion"));
        assertEquals("28", results.get("osversion"));

        assertTrue(results.containsKey("experiments"));
        final JSONObject experiments = (JSONObject) results.get("experiments");
        assertEquals(0, experiments.length());
    }

    @Test
    public void testPingWithExperiments() throws Exception {
        final TelemetryConfiguration configuration = new TelemetryConfiguration(RuntimeEnvironment.application);
        final TelemetryMobileEventPingBuilder builder = new TelemetryMobileEventPingBuilder(configuration);

        final Map<String, Boolean> experiments = new HashMap<>();
        experiments.put("use-gecko", true);
        experiments.put("use-homescreen-tips", false);
        builder.getExperimentsMapMeasurement().setExperiments(experiments);

        final TelemetryPing ping = builder.build();

        final Map<String, Object> results = ping.getMeasurementResults();
        assertNotNull(results);

        assertTrue(results.containsKey("experiments"));
        final JSONObject values = (JSONObject) results.get("experiments");
        assertEquals(2, values.length());
        assertEquals("branch", values.get("use-gecko"));
        assertEquals("control", values.get("use-homescreen-tips"));
    }

    @Test
    public void testSequenceNumber() {
        final TelemetryConfiguration configuration = new TelemetryConfiguration(RuntimeEnvironment.application);
        final TelemetryMobileEventPingBuilder builder = new TelemetryMobileEventPingBuilder(configuration);

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