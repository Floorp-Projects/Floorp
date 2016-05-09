/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, you can obtain one at http://mozilla.org/MPL/2.0/.
 */

package org.mozilla.gecko.util;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.background.testhelpers.TestRunner;

import java.util.Calendar;
import java.util.GregorianCalendar;
import java.util.Locale;
import java.util.TimeZone;

import static org.junit.Assert.assertEquals;

/**
 * Unit tests for date utilities.
 */
@RunWith(TestRunner.class)
public class TestDateUtil {
    @Test
    public void testGetDateInHTTPFormatGMT() {
        final TimeZone gmt = TimeZone.getTimeZone("GMT");
        final GregorianCalendar calendar = new GregorianCalendar(gmt, Locale.US);
        calendar.set(2011, Calendar.FEBRUARY, 1, 14, 0, 0);
        final String expectedDate = "Tue, 01 Feb 2011 14:00:00 GMT";

        final String actualDate = DateUtil.getDateInHTTPFormat(calendar.getTime());
        assertEquals("Returned date is expected", expectedDate, actualDate);
    }

    @Test
    public void testGetDateInHTTPFormatNonGMT() {
        final TimeZone kst = TimeZone.getTimeZone("Asia/Seoul"); // no daylight savings time.
        final GregorianCalendar calendar = new GregorianCalendar(kst, Locale.US);
        calendar.set(2011, Calendar.FEBRUARY, 1, 14, 0, 0);
        final String expectedDate = "Tue, 01 Feb 2011 05:00:00 GMT";

        final String actualDate = DateUtil.getDateInHTTPFormat(calendar.getTime());
        assertEquals("Returned date is expected", expectedDate, actualDate);
    }

    @Test
    public void testGetTimezoneOffsetInMinutes() {
        assertEquals("GMT has no offset", 0, DateUtil.getTimezoneOffsetInMinutes(TimeZone.getTimeZone("GMT")));

        // We use custom timezones because they don't have daylight savings time.
        assertEquals("Offset for GMT-8 is correct",
                -480, DateUtil.getTimezoneOffsetInMinutes(TimeZone.getTimeZone("GMT-8")));
        assertEquals("Offset for GMT+12:45 is correct",
                765, DateUtil.getTimezoneOffsetInMinutes(TimeZone.getTimeZone("GMT+12:45")));

        // We use a non-custom timezone without DST.
        assertEquals("Offset for KST is correct",
                540, DateUtil.getTimezoneOffsetInMinutes(TimeZone.getTimeZone("Asia/Seoul")));
    }

    @Test
    public void testGetTimezoneOffsetInMinutesForGivenDateNoDaylightSavingsTime() {
        final TimeZone kst = TimeZone.getTimeZone("Asia/Seoul");
        final Calendar[] calendars =
                new Calendar[] { getCalendarForMonth(Calendar.DECEMBER), getCalendarForMonth(Calendar.AUGUST) };
        for (final Calendar cal : calendars) {
            cal.setTimeZone(kst);
            assertEquals("Offset for KST does not change with daylight savings time",
                    540, DateUtil.getTimezoneOffsetInMinutesForGivenDate(cal));
        }
    }

    @Test
    public void testGetTimezoneOffsetInMinutesForGivenDateDaylightSavingsTime() {
        final TimeZone pacificTimeZone = TimeZone.getTimeZone("America/Los_Angeles");
        final Calendar pstCalendar = getCalendarForMonth(Calendar.DECEMBER);
        final Calendar pdtCalendar = getCalendarForMonth(Calendar.AUGUST);
        pstCalendar.setTimeZone(pacificTimeZone);
        pdtCalendar.setTimeZone(pacificTimeZone);
        assertEquals("Offset for PST is correct", -480, DateUtil.getTimezoneOffsetInMinutesForGivenDate(pstCalendar));
        assertEquals("Offset for PDT is correct", -420, DateUtil.getTimezoneOffsetInMinutesForGivenDate(pdtCalendar));

    }

    private Calendar getCalendarForMonth(final int month) {
        return new GregorianCalendar(2000, month, 1);
    }
}
