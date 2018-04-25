/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.telemetry.measurement;

import android.content.SharedPreferences;

import org.mozilla.telemetry.config.TelemetryConfiguration;

import java.util.UUID;

/**
 * A unique, randomly generated UUID for this client.
 */
public class ClientIdMeasurement extends TelemetryMeasurement {
    private static final String FIELD_NAME = "clientId";

    private static final String PREFERENCE_CLIENT_ID = "client_id";

    private TelemetryConfiguration configuration;
    private String clientId;

    public ClientIdMeasurement(TelemetryConfiguration configuration) {
        super(FIELD_NAME);

        this.configuration = configuration;
    }

    @Override
    public Object flush() {
        if (clientId == null) {
            clientId = generateClientId(configuration);
        }

        return clientId;
    }

    private static synchronized String generateClientId(final TelemetryConfiguration configuration) {
        final SharedPreferences preferences = configuration.getSharedPreferences();

        if (preferences.contains(PREFERENCE_CLIENT_ID)) {
            // We already generated a client id in the past. Let's use it.
            return preferences.getString(PREFERENCE_CLIENT_ID, /* unused default value */ null);
        }

        final String clientId = UUID.randomUUID().toString();

        preferences.edit()
                .putString(PREFERENCE_CLIENT_ID, clientId)
                .apply();

        return clientId;
    }
}
