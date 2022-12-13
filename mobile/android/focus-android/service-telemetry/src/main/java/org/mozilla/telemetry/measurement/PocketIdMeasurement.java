/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.telemetry.measurement;

import android.content.SharedPreferences;

import org.mozilla.telemetry.config.TelemetryConfiguration;

import java.util.UUID;

/**
 * A unique, randomly generated UUID for this pocket client for fire-tv instance.
 * This is distinct from the telemetry clientId. The clientId should not be able to be tied to the pocketId in any way.
 */
public class PocketIdMeasurement extends TelemetryMeasurement {
    private static final String FIELD_NAME = "pocketId";

    private static final String PREFERENCE_POCKET_ID = "pocket_id";

    private TelemetryConfiguration configuration;
    private String pocketId;

    public PocketIdMeasurement(TelemetryConfiguration configuration) {
        super(FIELD_NAME);

        this.configuration = configuration;
    }

    @Override
    public Object flush() {
        if (pocketId == null) {
            pocketId = generateClientId(configuration);
        }

        return pocketId;
    }

    private static synchronized String generateClientId(final TelemetryConfiguration configuration) {
        final SharedPreferences preferences = configuration.getSharedPreferences();

        if (preferences.contains(PREFERENCE_POCKET_ID)) {
            // We already generated a pocket id in the past. Let's use it.
            return preferences.getString(PREFERENCE_POCKET_ID, /* unused default value */ null);
        }

        final String pocketId = UUID.randomUUID().toString();

        preferences.edit()
                .putString(PREFERENCE_POCKET_ID, pocketId)
                .apply();

        return pocketId;
    }
}
