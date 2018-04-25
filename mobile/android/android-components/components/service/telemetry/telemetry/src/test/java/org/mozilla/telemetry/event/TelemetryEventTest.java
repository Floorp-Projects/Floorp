/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.telemetry.event;

import android.text.TextUtils;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.telemetry.Telemetry;
import org.mozilla.telemetry.TelemetryHolder;
import org.mozilla.telemetry.TestUtils;
import org.mozilla.telemetry.config.TelemetryConfiguration;
import org.mozilla.telemetry.measurement.EventsMeasurement;
import org.mozilla.telemetry.net.TelemetryClient;
import org.mozilla.telemetry.ping.TelemetryEventPingBuilder;
import org.mozilla.telemetry.ping.TelemetryMobileEventPingBuilder;
import org.mozilla.telemetry.schedule.TelemetryScheduler;
import org.mozilla.telemetry.storage.TelemetryStorage;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;

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
    public void testQueuing() throws Exception {
        final EventsMeasurement measurement = mock(EventsMeasurement.class);

        final TelemetryMobileEventPingBuilder builder = mock(TelemetryMobileEventPingBuilder.class);
        doReturn(TelemetryMobileEventPingBuilder.TYPE).when(builder).getType();
        doReturn(measurement).when(builder).getEventsMeasurement();

        final TelemetryConfiguration configuration = new TelemetryConfiguration(RuntimeEnvironment.application);
        final TelemetryStorage storage = mock(TelemetryStorage.class);
        final TelemetryClient client = mock(TelemetryClient.class);
        final TelemetryScheduler scheduler = mock(TelemetryScheduler.class);

        final Telemetry telemetry = new Telemetry(configuration, storage, client, scheduler)
                .addPingBuilder(builder);

        TelemetryHolder.set(telemetry);

        final TelemetryEvent event = TelemetryEvent.create("action", "type_url", "search_bar");
        event.queue();

        TestUtils.waitForExecutor(telemetry);

        verify(measurement).add(event);

        TelemetryHolder.set(null);
    }

    @Test
    @Deprecated // If you change this test, change the one above it too.
    public void testQueuingLegacyPingType() throws Exception {
        final EventsMeasurement measurement = mock(EventsMeasurement.class);

        final TelemetryEventPingBuilder builder = mock(TelemetryEventPingBuilder.class);
        doReturn(TelemetryEventPingBuilder.TYPE).when(builder).getType();
        doReturn(measurement).when(builder).getEventsMeasurement();

        final TelemetryConfiguration configuration = new TelemetryConfiguration(RuntimeEnvironment.application);
        final TelemetryStorage storage = mock(TelemetryStorage.class);
        final TelemetryClient client = mock(TelemetryClient.class);
        final TelemetryScheduler scheduler = mock(TelemetryScheduler.class);

        final Telemetry telemetry = new Telemetry(configuration, storage, client, scheduler)
                .addPingBuilder(builder);

        TelemetryHolder.set(telemetry);

        final TelemetryEvent event = TelemetryEvent.create("action", "type_url", "search_bar");
        event.queue();

        TestUtils.waitForExecutor(telemetry);

        verify(measurement).add(event);

        TelemetryHolder.set(null);
    }
}