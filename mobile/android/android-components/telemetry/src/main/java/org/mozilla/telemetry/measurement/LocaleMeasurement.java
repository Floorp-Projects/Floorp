/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.telemetry.measurement;

import java.util.Locale;

public class LocaleMeasurement extends TelemetryMeasurement {
    private static final String FIELD_NAME = "locale";

    public LocaleMeasurement() {
        super(FIELD_NAME);
    }

    @Override
    public Object flush() {
        Locale locale = Locale.getDefault();

        // TODO: There are multiple problems with this (Issue #18):
        // (1) It might not work with apps where we ship our own locale switcher. We might want
        //     to get the current locale from somewhere else (or provide a way to override what
        //     this measurement returns).
        // (2) getLanguage() returns an ISO 639 language code. ISO 639 is a moving target and we
        //     may return different codes for the same language. We probably want to fix this here
        //     or at least make this known to the server side processing those pings.
        return locale.getLanguage() + "-" + locale.getCountry();
    }
}
