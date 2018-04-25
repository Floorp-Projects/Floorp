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
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.only;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

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

    @Test
    public void testSettingsProviderIsUsed() {
        final SettingsMeasurement.SettingsProvider settingsProvider = mock(SettingsMeasurement.SettingsProvider.class);
        doReturn(true).when(settingsProvider).containsKey(anyString());
        doReturn("value").when(settingsProvider).getValue(anyString());

        final TelemetryConfiguration configuration = new TelemetryConfiguration(RuntimeEnvironment.application)
                .setSettingsProvider(settingsProvider)
                .setPreferencesImportantForTelemetry("a", "b", "c");

        final SettingsMeasurement measurement = new SettingsMeasurement(configuration);

        measurement.flush();

        verify(settingsProvider).update(configuration);

        verify(settingsProvider).containsKey("a");
        verify(settingsProvider).getValue("a");

        verify(settingsProvider).containsKey("b");
        verify(settingsProvider).getValue("b");

        verify(settingsProvider).containsKey("c");
        verify(settingsProvider).getValue("c");

        verify(settingsProvider).release();
    }

    @Test
    public void testCustomSettingsProvider() throws Exception {
        // A custom settings provider that will return for an integer key x the value x * 2. For all
        // not even integer keys it will pretent that they are not in the settings.
        final SettingsMeasurement.SettingsProvider settingsProvider = new SettingsMeasurement.SettingsProvider() {
            @Override
            public void update(TelemetryConfiguration configuration) {}

            @Override
            public boolean containsKey(String key) {
                return Integer.parseInt(key) % 2 == 0;
            }

            @Override
            public Object getValue(String key) {
                return Integer.parseInt(key) * 2;
            }

            @Override
            public void release() {}
        };

        final TelemetryConfiguration configuration = new TelemetryConfiguration(RuntimeEnvironment.application)
                .setSettingsProvider(settingsProvider)
                .setPreferencesImportantForTelemetry("1", "2", "3", "4");

        final SettingsMeasurement measurement = new SettingsMeasurement(configuration);

        measurement.flush();

        final Object value = measurement.flush();

        assertNotNull(value);
        assertTrue(value instanceof JSONObject);

        final JSONObject object = (JSONObject) value;

        assertEquals(4, object.length());

        assertTrue(object.has("1"));
        assertTrue(object.isNull("1"));

        assertTrue(object.has("2"));
        assertTrue(object.get("2") instanceof String);
        assertEquals("4", object.get("2"));

        assertTrue(object.has("3"));
        assertTrue(object.isNull("3"));

        assertTrue(object.has("4"));
        assertTrue(object.get("4") instanceof String);
        assertEquals("8", object.get("4"));
    }
}
