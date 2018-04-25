/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.telemetry.measurement;

import org.json.JSONArray;
import org.json.JSONObject;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.telemetry.config.TelemetryConfiguration;
import org.mozilla.telemetry.event.TelemetryEvent;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;

import java.util.HashMap;
import java.util.Map;
import java.util.UUID;

import static org.junit.Assert.*;

@RunWith(RobolectricTestRunner.class)
public class EventsMeasurementTest {
    @Test
    public void testStoringEventsAndFlushing() throws Exception {
        final TelemetryConfiguration configuration = new TelemetryConfiguration(RuntimeEnvironment.application);
        final EventsMeasurement measurement = new EventsMeasurement(configuration);

        final String category = "category828";
        final String method = "method910";
        final String object = "object010";

        final TelemetryEvent event = TelemetryEvent.create(category, method, object);
        measurement.add(event);

        final Object value = measurement.flush();

        assertNotNull(value);
        assertTrue(value instanceof JSONArray);

        final JSONArray array = (JSONArray) value;

        assertEquals(1, array.length());

        final JSONArray flushedEvent = array.getJSONArray(0);

        assertEquals(category, flushedEvent.getString(1));
        assertEquals(method, flushedEvent.getString(2));
        assertEquals(object, flushedEvent.getString(3));
    }

    @Test
    public void testFlushingEmptyMeasurement() {
        final TelemetryConfiguration configuration = new TelemetryConfiguration(RuntimeEnvironment.application);
        final EventsMeasurement measurement = new EventsMeasurement(configuration);

        final Object value = measurement.flush();

        assertNotNull(value);
        assertTrue(value instanceof JSONArray);

        JSONArray array = (JSONArray) value;

        assertEquals(0, array.length());
    }

    @Test
    public void testLocalFileIsDeletedAfterFlushing() throws Exception {
        final TelemetryConfiguration configuration = new TelemetryConfiguration(RuntimeEnvironment.application);
        final EventsMeasurement measurement = new EventsMeasurement(configuration);

        assertFalse(measurement.getEventFile().exists());

        final TelemetryEvent event = TelemetryEvent.create("category", "method", "object");
        measurement.add(event);

        assertTrue(measurement.getEventFile().exists());

        measurement.flush();

        assertFalse(measurement.getEventFile().exists());
    }

    @Test
    public void testToJSONCreatesArray() throws Exception {
        final TelemetryConfiguration configuration = new TelemetryConfiguration(RuntimeEnvironment.application);
        final EventsMeasurement measurement = new EventsMeasurement(configuration);

        measurement.add(TelemetryEvent.create("action", "type_url", "search_bar"));
        measurement.add(TelemetryEvent.create("action", "click", "erase_button"));

        final Object value = measurement.flush();
        assertNotNull(value);
        assertTrue(value instanceof JSONArray);

        final JSONArray events = (JSONArray) value;
        assertEquals(2, events.length());

        final JSONArray event1 = events.getJSONArray(0);
        assertEquals(4, event1.length());
        assertTrue(event1.getLong(0) >= 0);
        assertEquals("action", event1.getString(1));
        assertEquals("type_url", event1.getString(2));
        assertEquals("search_bar", event1.getString(3));

        final JSONArray event2 = events.getJSONArray(1);
        assertEquals(4, event2.length());
        assertTrue(event2.getLong(0) >= 0);
        assertEquals("action", event2.getString(1));
        assertEquals("click", event2.getString(2));
        assertEquals("erase_button", event2.getString(3));
    }

    @Test
    public void testToJSONWithComplexEvent() throws Exception {
        final TelemetryConfiguration configuration = new TelemetryConfiguration(RuntimeEnvironment.application);
        final EventsMeasurement measurement = new EventsMeasurement(configuration);

        measurement
                .add(TelemetryEvent.create("action", "change", "setting", "pref_search_engine")
                    .extra("from", "Google")
                    .extra("to", "Yahoo"));

        final Object value = measurement.flush();
        assertNotNull(value);
        assertTrue(value instanceof JSONArray);

        final JSONArray events = (JSONArray) value;
        assertEquals(1, events.length());

        final JSONArray event = events.getJSONArray(0);
        assertEquals(6, event.length());
        assertTrue(event.getLong(0) >= 0);
        assertEquals("action", event.getString(1));
        assertEquals("change", event.getString(2));
        assertEquals("setting", event.getString(3));
        assertEquals("pref_search_engine", event.getString(4));

        final JSONObject eventExtras = event.getJSONObject(5);
        assertEquals(2, eventExtras.length());
        assertTrue(eventExtras.has("from"));
        assertTrue(eventExtras.has("to"));
        assertEquals("Google", eventExtras.getString("from"));
        assertEquals("Yahoo", eventExtras.getString("to"));
    }
}