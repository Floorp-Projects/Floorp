/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.telemetry.measurement;

import android.preference.PreferenceManager;

import org.json.JSONException;
import org.json.JSONObject;
import org.mozilla.telemetry.config.TelemetryConfiguration;

import java.util.Map;
import java.util.Set;

public class SettingsMeasurement extends TelemetryMeasurement {
    private static final String FIELD_NAME = "settings";

    private final TelemetryConfiguration configuration;

    public SettingsMeasurement(TelemetryConfiguration configuration) {
        super(FIELD_NAME);

        this.configuration = configuration;
    }

    @Override
    public Object flush() {
        final JSONObject object = new JSONObject();

        final Set<String> preferenceKeys = configuration.getPreferencesImportantForTelemetry();
        if (preferenceKeys.isEmpty()) {
            return object;
        }

        final Map<String, ?> preferences = PreferenceManager.getDefaultSharedPreferences(
                configuration.getContext()).getAll();

        for (String key : preferenceKeys) {
            try {
                if (preferences.containsKey(key)) {
                    object.put(key, String.valueOf(preferences.get(key)));
                } else {
                    object.put(key, JSONObject.NULL);
                }
            } catch (JSONException e) {
                throw new AssertionError("Preference value can't be serialized to JSON", e);
            }
        }

        return object;
    }
}
