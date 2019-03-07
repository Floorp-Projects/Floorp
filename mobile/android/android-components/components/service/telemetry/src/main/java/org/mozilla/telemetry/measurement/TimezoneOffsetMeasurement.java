/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.telemetry.measurement;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import java.util.Calendar;
import java.util.concurrent.TimeUnit;

public class TimezoneOffsetMeasurement extends TelemetryMeasurement {
    private static final String FIELD_NAME = "tz";

    public TimezoneOffsetMeasurement() {
        super(FIELD_NAME);
    }

    @Override
    public Object flush() {
        return getTimezoneOffsetInMinutesForGivenDate(now());
    }

    /**
     * Returns the time zone offset for the given date in minutes. The date makes a difference due to daylight
     * savings time in some regions. We return minutes because we can accurately represent time zones that are
     * offset by non-integer hour values, e.g. parts of New Zealand at UTC+12:45.
     *
     * @param calendar A calendar with the appropriate time zone & date already set.
     */
    private static int getTimezoneOffsetInMinutesForGivenDate(@NonNull final Calendar calendar) {
        // via Date.getTimezoneOffset deprecated docs (note: it had incorrect order of operations).
        // Also, we cast to int because we should never overflow here - the max should be GMT+14 = 840.
        return (int) TimeUnit.MILLISECONDS.toMinutes(calendar.get(Calendar.ZONE_OFFSET) + calendar.get(Calendar.DST_OFFSET));
    }

    @VisibleForTesting Calendar now() {
        return Calendar.getInstance();
    }
}

