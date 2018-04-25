/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.telemetry.measurement;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.telemetry.config.TelemetryConfiguration;
import org.mozilla.telemetry.ping.TelemetryPingBuilder;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;

import static org.junit.Assert.*;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;

@RunWith(RobolectricTestRunner.class)
public class SequenceMeasurementTest {
    @Test
    public void testSequenceStartsAtOne() {
        final TelemetryPingBuilder builder = mock(TelemetryPingBuilder.class);
        doReturn("test").when(builder).getType();

        final TelemetryConfiguration configuration = new TelemetryConfiguration(RuntimeEnvironment.application);

        final SequenceMeasurement measurement = new SequenceMeasurement(configuration, builder);

        final Object value = measurement.flush();
        assertNotNull(value);
        assertTrue(value instanceof Long);

        long sequence = (Long) value;
        assertEquals(1, sequence);
    }

    @Test
    public void testSequenceNumberIsIncremented() {
        final TelemetryPingBuilder builder = mock(TelemetryPingBuilder.class);
        doReturn("test").when(builder).getType();

        final TelemetryConfiguration configuration = new TelemetryConfiguration(RuntimeEnvironment.application);

        final SequenceMeasurement measurement = new SequenceMeasurement(configuration, builder);

        // (1)

        final Object first = measurement.flush();
        assertNotNull(first);
        assertTrue(first instanceof Long);

        long sequence1 = (Long) first;
        assertEquals(1, sequence1);

        // (2)

        final Object second = measurement.flush();
        assertNotNull(second);
        assertTrue(second instanceof Long);

        long sequence2 = (Long) second;
        assertEquals(2, sequence2);

        // (3)

        final Object third = measurement.flush();
        assertNotNull(third);
        assertTrue(third instanceof Long);

        long sequence3 = (Long) third;
        assertEquals(3, sequence3);
    }

    @Test
    public void testSamePingTypeSharesSequence() {
        final TelemetryPingBuilder builder = mock(TelemetryPingBuilder.class);
        doReturn("test").when(builder).getType();

        final TelemetryConfiguration configuration = new TelemetryConfiguration(RuntimeEnvironment.application);

        final SequenceMeasurement measurement1 = new SequenceMeasurement(configuration, builder);
        final SequenceMeasurement measurement2 = new SequenceMeasurement(configuration, builder);

        // Measurement #1 starts at 1

        final Object first = measurement1.flush();
        assertNotNull(first);
        assertTrue(first instanceof Long);

        long sequence1 = (Long) first;
        assertEquals(1, sequence1);

        // Flush measurement #2 three times

        measurement2.flush();
        measurement2.flush();
        measurement2.flush();

        // Measurement #1 is now at 5

        final Object second = measurement1.flush();
        assertNotNull(second);
        assertTrue(second instanceof Long);

        long sequence2 = (Long) second;
        assertEquals(5, sequence2);

        // And Measurement #2 continues with 6

        final Object third = measurement2.flush();
        assertNotNull(third);
        assertTrue(third instanceof Long);

        long sequence3 = (Long) third;
        assertEquals(6, sequence3);
    }

    @Test
    public void testDifferentPingsHaveTheirOwnSequence() {
        final TelemetryPingBuilder testBuilder1 = mock(TelemetryPingBuilder.class);
        doReturn("test1").when(testBuilder1).getType();

        final TelemetryPingBuilder testBuilder2 = mock(TelemetryPingBuilder.class);
        doReturn("test2").when(testBuilder2).getType();

        final TelemetryConfiguration configuration = new TelemetryConfiguration(RuntimeEnvironment.application);

        final SequenceMeasurement measurement1 = new SequenceMeasurement(configuration, testBuilder1);
        final SequenceMeasurement measurement2 = new SequenceMeasurement(configuration, testBuilder2);

        // Measurement #1 starts at 1

        final Object first = measurement1.flush();
        assertNotNull(first);
        assertTrue(first instanceof Long);

        long sequence1 = (Long) first;
        assertEquals(1, sequence1);

        // Flush measurement #2 three times

        measurement2.flush();
        measurement2.flush();
        measurement2.flush();

        // Measurement #1 continues on its own is now at 2

        final Object second = measurement1.flush();
        assertNotNull(second);
        assertTrue(second instanceof Long);

        long sequence2 = (Long) second;
        assertEquals(2, sequence2);

        // Measurement #2 is independ and is at 4 now

        final Object third = measurement2.flush();
        assertNotNull(third);
        assertTrue(third instanceof Long);

        long sequence3 = (Long) third;
        assertEquals(4, sequence3);
    }
}