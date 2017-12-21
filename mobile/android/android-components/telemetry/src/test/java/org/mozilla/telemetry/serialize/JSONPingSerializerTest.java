/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.telemetry.serialize;

import org.json.JSONArray;
import org.json.JSONObject;
import org.junit.FixMethodOrder;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.telemetry.config.TelemetryConfiguration;
import org.mozilla.telemetry.event.TelemetryEvent;
import org.mozilla.telemetry.measurement.EventsMeasurement;
import org.mozilla.telemetry.ping.TelemetryCorePingBuilder;
import org.mozilla.telemetry.ping.TelemetryEventPingBuilder;
import org.mozilla.telemetry.ping.TelemetryMobileEventPingBuilder;
import org.mozilla.telemetry.ping.TelemetryPing;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;

import java.util.UUID;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

@RunWith(RobolectricTestRunner.class)
public class JSONPingSerializerTest {
    @Test
    public void testSerializingEventPing() throws Exception {
        final TelemetryConfiguration configuration = new TelemetryConfiguration(RuntimeEnvironment.application);
        final TelemetryMobileEventPingBuilder builder = new TelemetryMobileEventPingBuilder(configuration);
        final TelemetryPing ping = builder.build();

        final TelemetryPingSerializer serializer = new JSONPingSerializer();

        final String serializedPing = serializer.serialize(ping);

        // This will throw if the serializer doesn't produce valid JSON.
        final JSONObject json = new JSONObject(serializedPing);

        assertTrue(json.has("clientId"));
        assertTrue(json.has("os"));
        assertTrue(json.has("osversion"));
        assertTrue(json.has("v"));
        assertTrue(json.has("tz"));
        assertTrue(json.has("os"));
        assertTrue(json.has("locale"));
        assertTrue(json.has("seq"));
        assertTrue(json.has("events"));
        assertTrue(json.has("settings"));

        assertEquals(1, json.getLong("seq"));

        final JSONArray events = json.getJSONArray("events");
        assertNotNull(events);
        assertEquals(0, events.length());

        final JSONObject settings = json.getJSONObject("settings");
        assertNotNull(settings);
        assertEquals(0, settings.length());

        final UUID clientId = UUID.fromString(json.getString("clientId"));
        assertNotNull(clientId);
        assertEquals(4, clientId.version());
    }

    @Test
    @Deprecated // If you change this test, change the one above it.
    public void testSerializingEventPingLegacyPingType() throws Exception {
        final TelemetryConfiguration configuration = new TelemetryConfiguration(RuntimeEnvironment.application);
        final TelemetryEventPingBuilder builder = new TelemetryEventPingBuilder(configuration);
        final TelemetryPing ping = builder.build();

        final TelemetryPingSerializer serializer = new JSONPingSerializer();

        final String serializedPing = serializer.serialize(ping);

        // This will throw if the serializer doesn't produce valid JSON.
        final JSONObject json = new JSONObject(serializedPing);

        assertTrue(json.has("clientId"));
        assertTrue(json.has("os"));
        assertTrue(json.has("osversion"));
        assertTrue(json.has("v"));
        assertTrue(json.has("tz"));
        assertTrue(json.has("os"));
        assertTrue(json.has("locale"));
        assertTrue(json.has("seq"));
        assertTrue(json.has("events"));
        assertTrue(json.has("settings"));

        assertEquals(1, json.getLong("seq"));

        final JSONArray events = json.getJSONArray("events");
        assertNotNull(events);
        assertEquals(0, events.length());

        final JSONObject settings = json.getJSONObject("settings");
        assertNotNull(settings);
        assertEquals(0, settings.length());

        final UUID clientId = UUID.fromString(json.getString("clientId"));
        assertNotNull(clientId);
        assertEquals(4, clientId.version());
    }

    @Test
    public void testSerializingEventPingWithEvents() throws Exception {
        final TelemetryConfiguration configuration = new TelemetryConfiguration(RuntimeEnvironment.application);
        final TelemetryMobileEventPingBuilder builder = new TelemetryMobileEventPingBuilder(configuration);

        final EventsMeasurement measurement = builder.getEventsMeasurement();
        measurement.add(TelemetryEvent.create("action", "open", "app"));
        measurement.add(TelemetryEvent.create("action", "type_url", "search_bar"));
        measurement.add(TelemetryEvent.create("action", "click", "erase_button"));

        final TelemetryPing ping = builder.build();
        final TelemetryPingSerializer serializer = new JSONPingSerializer();

        final String serializedPing = serializer.serialize(ping);

        final JSONObject json = new JSONObject(serializedPing);
        assertTrue(json.has("events"));

        final JSONArray events = json.getJSONArray("events");
        assertEquals(3, events.length());

        final JSONArray event1 = events.getJSONArray(0);
        assertEquals("action", event1.getString(1));
        assertEquals("open", event1.getString(2));
        assertEquals("app", event1.getString(3));

        final JSONArray event2 = events.getJSONArray(1);
        assertEquals("action", event2.getString(1));
        assertEquals("type_url", event2.getString(2));
        assertEquals("search_bar", event2.getString(3));

        final JSONArray event3 = events.getJSONArray(2);
        assertEquals("action", event3.getString(1));
        assertEquals("click", event3.getString(2));
        assertEquals("erase_button", event3.getString(3));
    }

    @Test
    @Deprecated // If you change this test, change the one above it.
    public void testSerializingEventPingWithEventsLegacyPingType() throws Exception {
        final TelemetryConfiguration configuration = new TelemetryConfiguration(RuntimeEnvironment.application);
        final TelemetryEventPingBuilder builder = new TelemetryEventPingBuilder(configuration);

        final EventsMeasurement measurement = builder.getEventsMeasurement();
        measurement.add(TelemetryEvent.create("action", "open", "app"));
        measurement.add(TelemetryEvent.create("action", "type_url", "search_bar"));
        measurement.add(TelemetryEvent.create("action", "click", "erase_button"));

        final TelemetryPing ping = builder.build();
        final TelemetryPingSerializer serializer = new JSONPingSerializer();

        final String serializedPing = serializer.serialize(ping);

        final JSONObject json = new JSONObject(serializedPing);
        assertTrue(json.has("events"));

        final JSONArray events = json.getJSONArray("events");
        assertEquals(3, events.length());

        final JSONArray event1 = events.getJSONArray(0);
        assertEquals("action", event1.getString(1));
        assertEquals("open", event1.getString(2));
        assertEquals("app", event1.getString(3));

        final JSONArray event2 = events.getJSONArray(1);
        assertEquals("action", event2.getString(1));
        assertEquals("type_url", event2.getString(2));
        assertEquals("search_bar", event2.getString(3));

        final JSONArray event3 = events.getJSONArray(2);
        assertEquals("action", event3.getString(1));
        assertEquals("click", event3.getString(2));
        assertEquals("erase_button", event3.getString(3));
    }

    @Test
    public void testSerializingCorePing() throws Exception {
        final TelemetryConfiguration configuration = new TelemetryConfiguration(RuntimeEnvironment.application);
        final TelemetryCorePingBuilder builder = new TelemetryCorePingBuilder(configuration);
        final TelemetryPing ping = builder.build();

        final TelemetryPingSerializer serializer = new JSONPingSerializer();

        final String serializedPing = serializer.serialize(ping);

        final JSONObject json = new JSONObject(serializedPing);

        assertTrue(json.has("clientId"));
        assertTrue(json.has("os"));
        assertTrue(json.has("osversion"));
        assertTrue(json.has("v"));
        assertTrue(json.has("tz"));
        assertTrue(json.has("os"));
        assertTrue(json.has("locale"));
        assertTrue(json.has("seq"));

        assertEquals(1, json.getLong("seq"));

        final UUID clientId = UUID.fromString(json.getString("clientId"));
        assertNotNull(clientId);
        assertEquals(4, clientId.version());
    }
}
