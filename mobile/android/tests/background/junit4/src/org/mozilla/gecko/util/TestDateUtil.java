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
}
