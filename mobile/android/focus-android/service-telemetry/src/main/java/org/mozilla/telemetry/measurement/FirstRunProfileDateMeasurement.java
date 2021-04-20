/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.telemetry.measurement;

import android.content.SharedPreferences;
import androidx.annotation.VisibleForTesting;

import org.mozilla.telemetry.config.TelemetryConfiguration;

import java.util.concurrent.TimeUnit;

/**
 * This measurement will save the timestamp of the first time it was instantiated and report this
 * as profile creation date.
 */
public class FirstRunProfileDateMeasurement extends TelemetryMeasurement {
    private static final String FIELD_NAME = "profileDate";

    private static final String PREFERENCE_KEY = "profile_date";

    private TelemetryConfiguration configuration;

    public FirstRunProfileDateMeasurement(TelemetryConfiguration configuration) {
        super(FIELD_NAME);

        this.configuration = configuration;

        ensureValueExists();
    }

    @VisibleForTesting void ensureValueExists() {
        final SharedPreferences preferences = configuration.getSharedPreferences();

        if (preferences.contains(PREFERENCE_KEY)) {
            return;
        }

        preferences.edit()
                .putLong(PREFERENCE_KEY, now())
                .apply();
    }

    @Override
    public Object flush() {
        return getProfileDateInDays();
    }

    /**
     * Profile creation date in days since UNIX epoch.
     */
    private long getProfileDateInDays() {
        long profileDateMilliseconds = configuration.getSharedPreferences().getLong(PREFERENCE_KEY, now());

        return (long) Math.floor((double) profileDateMilliseconds / TimeUnit.DAYS.toMillis(1));
    }

    @VisibleForTesting long now() {
        return System.currentTimeMillis();
    }
}
