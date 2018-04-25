/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.telemetry.measurement;

import android.content.SharedPreferences;

import org.mozilla.telemetry.config.TelemetryConfiguration;

public class SessionCountMeasurement extends TelemetryMeasurement {
    private static final String FIELD_NAME = "sessions";

    private static final String PREFERENCE_COUNT = "session_count";

    private final TelemetryConfiguration configuration;

    public SessionCountMeasurement(TelemetryConfiguration configuration) {
        super(FIELD_NAME);

        this.configuration = configuration;
    }

    public synchronized void countSession() {
        final SharedPreferences preferences = configuration.getSharedPreferences();

        long count = preferences.getLong(PREFERENCE_COUNT, 0);

        preferences.edit()
                .putLong(PREFERENCE_COUNT, ++count)
                .apply();
    }

    @Override
    public Object flush() {
        return getAndResetCount();
    }

    private synchronized long getAndResetCount() {
        final SharedPreferences preferences = configuration.getSharedPreferences();

        long count = preferences.getLong(PREFERENCE_COUNT, 0);

        preferences.edit()
                .putLong(PREFERENCE_COUNT, 0)
                .apply();

        return count;
    }
}
