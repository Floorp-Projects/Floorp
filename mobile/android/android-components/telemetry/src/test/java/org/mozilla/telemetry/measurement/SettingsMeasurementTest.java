/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.telemetry.measurement;

import android.content.SharedPreferences;
import android.preference.PreferenceManager;
import android.util.ArraySet;

import org.json.JSONObject;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.telemetry.config.TelemetryConfiguration;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;

import java.util.Arrays;
import java.util.HashSet;
import java.util.UUID;

import static org.junit.Assert.*;

@RunWith(RobolectricTestRunner.class)
public class SettingsMeasurementTest {
    @Test
    public void testDefaultBehavior() {
        final TelemetryConfiguration configuration = new TelemetryConfiguration(RuntimeEnvironment.application);
        final SettingsMeasurement measurement = new SettingsMeasurement(configuration);

        final Object value = measurement.flush();

        assertNotNull(value);
        assertTrue(value instanceof JSONObject);

        final JSONObject object = (JSONObject) value;

        assertEquals(0, object.length());
    }

    @Test
    public void testWithConfiguredPreferences() throws Exception {
        final String keyPreferenceExisting = "pref_existing_" + UUID.randomUUID();
        final String keyPreferenceNeverWritten = "pref_never_written_" + UUID.randomUUID();

        final TelemetryConfiguration configuration = new TelemetryConfiguration(RuntimeEnvironment.application);
        configuration.setPreferencesImportantForTelemetry(
                keyPreferenceExisting,
                keyPreferenceNeverWritten
        );

        final SettingsMeasurement measurement = new SettingsMeasurement(configuration);

        final String preferenceValue = "value_" + UUID.randomUUID();

        SharedPreferences preferences = PreferenceManager.getDefaultSharedPreferences(RuntimeEnvironment.application);
        preferences.edit()
                .putString(keyPreferenceExisting, preferenceValue)
                .apply();

        final Object value = measurement.flush();

        assertNotNull(value);
        assertTrue(value instanceof JSONObject);

        final JSONObject object = (JSONObject) value;

        assertEquals(2, object.length());

        assertEquals(preferenceValue, object.get(keyPreferenceExisting));
        assertEquals(JSONObject.NULL, object.get(keyPreferenceNeverWritten));
    }

    @Test
    public void testDifferentTypes() throws Exception {
        final String keyBooleanPreference = "booleanPref";
        final String keyLongPreference = "longPref";
        final String keyStringPreference = "stringPref";
        final String keyIntPreference = "intPref";
        final String keyFloatPreference = "floatPref";
        final String keyStringSetPreference = "stringSetPref";

        PreferenceManager.getDefaultSharedPreferences(RuntimeEnvironment.application)
                .edit()
                .putBoolean(keyBooleanPreference, true)
                .putLong(keyLongPreference, 1337)
                .putString(keyStringPreference, "Hello Telemetry")
                .putInt(keyIntPreference, 42)
                .putFloat(keyFloatPreference, 23.45f)
                .putStringSet(keyStringSetPreference, new HashSet<>(Arrays.asList("chicken", "waffles")))
                .apply();

        final TelemetryConfiguration configuration = new TelemetryConfiguration(RuntimeEnvironment.application)
                .setPreferencesImportantForTelemetry(
                        keyBooleanPreference,
                        keyLongPreference,
                        keyStringPreference,
                        keyIntPreference,
                        keyFloatPreference,
                        keyStringSetPreference
                );

        final SettingsMeasurement measurement = new SettingsMeasurement(configuration);

        final Object value = measurement.flush();

        assertNotNull(value);
        assertTrue(value instanceof JSONObject);

        final JSONObject object = (JSONObject) value;

        assertEquals(6, object.length());

        assertTrue(object.get(keyBooleanPreference) instanceof String);
        assertEquals("true", object.get(keyBooleanPreference));

        assertTrue(object.get(keyLongPreference) instanceof String);
        assertEquals("1337", object.get(keyLongPreference));

        assertTrue(object.get(keyStringPreference) instanceof String);
        assertEquals("Hello Telemetry", object.get(keyStringPreference));

        assertTrue(object.get(keyIntPreference) instanceof String);
        assertEquals("42", object.get(keyIntPreference));

        assertTrue(object.get(keyFloatPreference) instanceof String);
        assertEquals("23.45", object.get(keyFloatPreference));

        assertTrue(object.get(keyStringSetPreference) instanceof String);
        assertEquals("[chicken, waffles]", object.get(keyStringSetPreference));
    }
}
