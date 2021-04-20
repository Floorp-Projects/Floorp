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
    /**
     * A generic interface for implementations that can provide settings values.
     */
    public interface SettingsProvider {
        /**
         * Notify this provider that we are going to read values from it. Some providers might need
         * to perform some actions to be able to provide a fresh set of values.
         */
        void update(TelemetryConfiguration configuration);

        /**
         * Returns true if a settings value is available for the given key.
         */
        boolean containsKey(String key);

        /**
         * Get the setting value for the given key.
         */
        Object getValue(String key);

        /**
         * Notify the provider that we finished reading from it and that it can release resources now.
         */
        void release();
    }

    /**
     * Setting provider implementation that reads values from SharedPreferences.
     */
    public static class SharedPreferenceSettingsProvider implements SettingsProvider {
        private Map<String, ?> preferences;

        @Override
        public void update(TelemetryConfiguration configuration) {
            preferences = PreferenceManager.getDefaultSharedPreferences(
                    configuration.getContext()).getAll();
        }

        @Override
        public boolean containsKey(String key) {
            return preferences != null && preferences.containsKey(key);
        }

        @Override
        public Object getValue(String key) {
            return preferences.get(key);
        }

        @Override
        public void release() {
            preferences = null;
        }
    }

    private static final String FIELD_NAME = "settings";

    private final TelemetryConfiguration configuration;

    public SettingsMeasurement(TelemetryConfiguration configuration) {
        super(FIELD_NAME);

        this.configuration = configuration;
    }

    @Override
    public Object flush() {
        final SettingsProvider settingsProvider = configuration.getSettingsProvider();
        settingsProvider.update(configuration);

        final JSONObject object = new JSONObject();

        final Set<String> preferenceKeys = configuration.getPreferencesImportantForTelemetry();
        if (preferenceKeys.isEmpty()) {
            return object;
        }

        for (String key : preferenceKeys) {
            try {
                if (settingsProvider.containsKey(key)) {
                    object.put(key, String.valueOf(settingsProvider.getValue(key)));
                } else {
                    object.put(key, JSONObject.NULL);
                }
            } catch (JSONException e) {
                throw new AssertionError("Preference value can't be serialized to JSON", e);
            }
        }

        settingsProvider.release();

        return object;
    }
}
