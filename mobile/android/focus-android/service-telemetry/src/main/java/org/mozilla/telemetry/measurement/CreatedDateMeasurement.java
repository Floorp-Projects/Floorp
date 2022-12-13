/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.telemetry.measurement;

import java.text.DateFormat;
import java.text.SimpleDateFormat;
import java.util.Calendar;
import java.util.Locale;

public class CreatedDateMeasurement extends TelemetryMeasurement {
    private static final String FIELD_NAME = "created";

    public CreatedDateMeasurement() {
        super(FIELD_NAME);
    }

    @Override
    public Object flush() {
        final DateFormat pingCreationDateFormat = new SimpleDateFormat("yyyy-MM-dd", Locale.US);
        final Calendar nowCalendar = Calendar.getInstance();

        return pingCreationDateFormat.format(nowCalendar.getTime());
    }
}
