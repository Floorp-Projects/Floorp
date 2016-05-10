/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, you can obtain one at http://mozilla.org/MPL/2.0/.
 */

package org.mozilla.gecko.util;

import android.support.annotation.NonNull;

import java.text.DateFormat;
import java.text.SimpleDateFormat;
import java.util.Calendar;
import java.util.Date;
import java.util.Locale;
import java.util.TimeZone;
import java.util.concurrent.TimeUnit;

/**
 * Utilities to help with manipulating Java's dates and calendars.
 */
public class DateUtil {
    private DateUtil() {}

    /**
     * @param date the date to convert to HTTP format
     * @return the date as specified in rfc 1123, e.g. "Tue, 01 Feb 2011 14:00:00 GMT"
     */
    public static String getDateInHTTPFormat(@NonNull final Date date) {
        final DateFormat df = new SimpleDateFormat("E, dd MMM yyyy HH:mm:ss z", Locale.US);
        df.setTimeZone(TimeZone.getTimeZone("GMT"));
        return df.format(date);
    }

    /**
     * Returns the timezone offset for the current date in minutes. See
     * {@link #getTimezoneOffsetInMinutesForGivenDate(Calendar)} for more details.
     */
    public static int getTimezoneOffsetInMinutes(@NonNull final TimeZone timezone) {
        return getTimezoneOffsetInMinutesForGivenDate(Calendar.getInstance(timezone));
    }

    /**
     * Returns the time zone offset for the given date in minutes. The date makes a difference due to daylight
     * savings time in some regions. We return minutes because we can accurately represent time zones that are
     * offset by non-integer hour values, e.g. parts of New Zealand at UTC+12:45.
     *
     * @param calendar A calendar with the appropriate time zone & date already set.
     */
    public static int getTimezoneOffsetInMinutesForGivenDate(@NonNull final Calendar calendar) {
        // via Date.getTimezoneOffset deprecated docs (note: it had incorrect order of operations).
        // Also, we cast to int because we should never overflow here - the max should be GMT+14 = 840.
        return (int) TimeUnit.MILLISECONDS.toMinutes(calendar.get(Calendar.ZONE_OFFSET) + calendar.get(Calendar.DST_OFFSET));
    }
}
