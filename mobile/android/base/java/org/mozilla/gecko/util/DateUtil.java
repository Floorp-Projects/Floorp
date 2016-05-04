/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, you can obtain one at http://mozilla.org/MPL/2.0/.
 */

package org.mozilla.gecko.util;

import android.support.annotation.NonNull;

import java.text.DateFormat;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Locale;
import java.util.TimeZone;

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
}
