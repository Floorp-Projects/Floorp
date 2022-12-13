/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.telemetry.measurement;

import android.content.SharedPreferences;
import androidx.annotation.NonNull;

import org.json.JSONException;
import org.json.JSONObject;
import org.mozilla.telemetry.config.TelemetryConfiguration;

import java.util.Collections;
import java.util.HashSet;
import java.util.Set;

/**
 * A TelemetryMeasurement implementation to count the number of times a user has searched with a
 * specific engine from a specific location.
 */
public class SearchesMeasurement extends TelemetryMeasurement {
    private static final String FIELD_NAME = "searches";

    private static final String PREFERENCE_SEARCH_KEYSET = "measurements-search-count-keyset";
    private static final String PREFERENCE_SEARCH_PREFIX = "measurements-search-count-engine-";

    public static final String LOCATION_ACTIONBAR = "actionbar";
    public static final String LOCATION_SUGGESTION = "suggestion";
    public static final String LOCATION_LISTITEM = "listitem";

    private final TelemetryConfiguration configuration;

    public SearchesMeasurement(TelemetryConfiguration configuration) {
        super(FIELD_NAME);

        this.configuration = configuration;
    }

    @Override
    public Object flush() {
        return getSearchCountMapAndReset();
    }

    /**
     * Get the stored search counts and reset all of them.
     */
    private synchronized JSONObject getSearchCountMapAndReset() {
        try {
            final SharedPreferences preferences = configuration.getSharedPreferences();
            final SharedPreferences.Editor editor = preferences.edit();

            final JSONObject object = new JSONObject();

            final Set<String> keys = preferences.getStringSet(PREFERENCE_SEARCH_KEYSET, Collections.<String>emptySet());
            for (String locationAndIdentifier : keys) {
                final String key = getEngineSearchCountKey(locationAndIdentifier);

                object.put(locationAndIdentifier, preferences.getInt(key, 0));
                editor.remove(key);
            }

            editor.remove(PREFERENCE_SEARCH_KEYSET)
                    .apply();

            return object;
        } catch (JSONException e) {
            throw new AssertionError("Should not happen: Can't construct search count JSON", e);
        }
    }

    /**
     * Record a search for the given location and search engine identifier.
     *
     * @param location where search was started.
     * @param identifier of the used search engine.
     */
    public synchronized void recordSearch(@NonNull String location, @NonNull String identifier) {
        final String locationAndIdentifier = location + "." + identifier;

        storeCount(locationAndIdentifier);
        storeKey(locationAndIdentifier);
    }

    /**
     * Store count for this engine in preferences.
     */
    private void storeCount(String locationAndIdentifier) {
        final SharedPreferences preferences = configuration.getSharedPreferences();

        final String key = getEngineSearchCountKey(locationAndIdentifier);

        final int count = preferences.getInt(key, 0);

        preferences.edit()
                .putInt(key, count + 1)
                .apply();
    }

    /**
     * Store name of this engine in preferences so that we can retrieve the count later.
     */
    private void storeKey(String locationAndIdentifier) {
        final SharedPreferences preferences = configuration.getSharedPreferences();

        final Set<String> keys = preferences.getStringSet(PREFERENCE_SEARCH_KEYSET, Collections.<String>emptySet());
        if (keys.contains(locationAndIdentifier)) {
            // We already know about this key.
            return;
        }

        // Create a copy that we can modify
        final Set<String> updatedKeys = new HashSet<>(keys);
        updatedKeys.add(locationAndIdentifier);

        preferences.edit()
                .putStringSet(PREFERENCE_SEARCH_KEYSET, updatedKeys)
                .apply();
    }

    private static String getEngineSearchCountKey(String locationAndIdentifier) {
        return PREFERENCE_SEARCH_PREFIX + locationAndIdentifier;
    }
}
