/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.telemetry.measurement;

import org.junit.Test;

import java.util.Calendar;
import java.util.TimeZone;

import static org.junit.Assert.*;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.spy;

public class TimezoneOffsetMeasurementTest {
    @Test
    public void testValueForUTC() {
        final TimezoneOffsetMeasurement measurement = spy(new TimezoneOffsetMeasurement());
        doReturn(createCalendarInTimeZone("UTC")).when(measurement).now();

        final Object value = measurement.flush();
        assertNotNull(value);
        assertTrue(value instanceof Integer);

        int offset = (Integer) value;
        assertEquals(0, offset); // There should be no offset if we are in UTC already
    }

    @Test
    public void testDifferentTimezones() {
        final TimezoneOffsetMeasurement measurement = spy(new TimezoneOffsetMeasurement());
        doReturn(createCalendarInTimeZone("GMT-8:00"))
                .doReturn(createCalendarInTimeZone("MST"))
                .doReturn(createCalendarInTimeZone("Australia/Darwin"))
                .doReturn(createCalendarInTimeZone("CEST"))
                .doReturn(createCalendarInTimeZone("GMT+2:00"))
                .when(measurement).now();

        assertEquals(-480, (int) measurement.flush());
        assertEquals(-420, (int) measurement.flush());
        assertEquals(570, (int) measurement.flush());
        assertEquals(0, (int) measurement.flush());
        assertEquals(120, (int) measurement.flush());
    }

    private Calendar createCalendarInTimeZone(String timzone) {
        return Calendar.getInstance(TimeZone.getTimeZone(timzone));
    }
}