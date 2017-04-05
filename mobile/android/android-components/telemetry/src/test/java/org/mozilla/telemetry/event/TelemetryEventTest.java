/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.telemetry.event;

import android.text.TextUtils;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.telemetry.Telemetry;
import org.mozilla.telemetry.TelemetryHolder;
import org.mozilla.telemetry.measurement.EventsMeasurement;
import org.mozilla.telemetry.ping.TelemetryEventPingBuilder;
import org.robolectric.RobolectricTestRunner;

import static org.junit.Assert.*;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;

@RunWith(RobolectricTestRunner.class)
public class TelemetryEventTest {
    @Test
    public void testEventCreation() {
        TelemetryEvent event = TelemetryEvent.create("action", "type_url", "search_bar");

        final String rawEvent = event.toJSON();

        assertNotNull(rawEvent);
        assertFalse(TextUtils.isEmpty(rawEvent));
        assertFalse(rawEvent.contains("\n"));
        assertFalse(rawEvent.contains("\r"));
        assertTrue(rawEvent.startsWith("["));
        assertTrue(rawEvent.endsWith("]"));
    }

    @Test
    public void testQueuing() {
        EventsMeasurement measurement = mock(EventsMeasurement.class);

        TelemetryEventPingBuilder builder = mock(TelemetryEventPingBuilder.class);
        doReturn(measurement).when(builder).getEventsMeasurement();

        Telemetry telemetry = mock(Telemetry.class);
        doReturn(builder).when(telemetry).getBuilder(TelemetryEventPingBuilder.TYPE);

        TelemetryHolder.set(telemetry);

        TelemetryEvent event = TelemetryEvent.create("action", "type_url", "search_bar");
        event.queue();

        verify(measurement).add(event);

        TelemetryHolder.set(null);
    }
}