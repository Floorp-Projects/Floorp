/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, you can obtain one at http://mozilla.org/MPL/2.0/.
 */

package org.mozilla.gecko.telemetry.measurements;

import android.content.SharedPreferences;
import android.support.annotation.NonNull;
import android.support.annotation.VisibleForTesting;
import org.mozilla.gecko.sync.ExtendedJSONObject;

import java.util.Collections;
import java.util.HashSet;
import java.util.Set;

/**
 * A place to store and retrieve the number of times a user has searched with a specific engine from a
 * specific location. This is designed for use as a telemetry core ping measurement.
 *
 * The implementation works by storing a preference for each engine-location pair and incrementing them
 * each time {@link #incrementSearch(SharedPreferences, String, String)} is called. In order to
 * retrieve the full set of keys later, we store all the available key names in another preference.
 *
 * When we retrieve the keys in {@link #getAndZeroSearch(SharedPreferences)} (using the set of keys
 * preference), the values saved to the preferences are returned and the preferences are removed
 * (i.e. zeroed) from Shared Preferences. The reason we remove the preferences (rather than actually
 * zeroing them) is to avoid bloating shared preferences if 1) the set of engines ever changes or
 * 2) we remove this feature.
 *
 * Since we increment a value on each successive search, which doesn't take up more space, we don't
 * have to worry about using excess disk space if the measurements are never zeroed (e.g. telemetry
 * upload is disabled). In the worst case, we overflow the integer and may return negative values.
 *
 * This class is thread-safe by locking access to its public methods. When this class was written, incrementing &
 * retrieval were called from multiple threads so rather than enforcing the callers keep their threads straight, it
 * was simpler to lock all access.
 */
public class SearchCountMeasurements {
    /** The set of "engine + where" keys we've stored; used for retrieving stored engines. */
    @VisibleForTesting static final String PREF_SEARCH_KEYSET = "measurements-search-count-keyset";
    private static final String PREF_SEARCH_PREFIX = "measurements-search-count-engine-"; // + "engine.where"

    private SearchCountMeasurements() {}

    public static synchronized void incrementSearch(@NonNull final SharedPreferences prefs,
            @NonNull final String engineIdentifier, @NonNull final String where) {
        final String engineWhereStr = engineIdentifier + "." + where;
        final String key = getEngineSearchCountKey(engineWhereStr);

        final int count = prefs.getInt(key, 0);
        prefs.edit().putInt(key, count + 1).apply();

        unionKeyToSearchKeyset(prefs, engineWhereStr);
    }

    /**
     * @param key Engine of the form, "engine.where"
     */
    private static void unionKeyToSearchKeyset(@NonNull final SharedPreferences prefs, @NonNull final String key) {
        final Set<String> keysFromPrefs = prefs.getStringSet(PREF_SEARCH_KEYSET, Collections.<String>emptySet());
        if (keysFromPrefs.contains(key)) {
            return;
        }

        // String set returned by shared prefs cannot be modified so we copy.
        final Set<String> keysToSave = new HashSet<>(keysFromPrefs);
        keysToSave.add(key);
        prefs.edit().putStringSet(PREF_SEARCH_KEYSET, keysToSave).apply();
    }

    /**
     * Gets and zeroes search counts.
     *
     * We return ExtendedJSONObject for now because that's the format needed by the core telemetry ping.
     */
    public static synchronized ExtendedJSONObject getAndZeroSearch(@NonNull final SharedPreferences prefs) {
        final ExtendedJSONObject out = new ExtendedJSONObject();
        final SharedPreferences.Editor editor = prefs.edit();

        final Set<String> keysFromPrefs = prefs.getStringSet(PREF_SEARCH_KEYSET, Collections.<String>emptySet());
        for (final String engineWhereStr : keysFromPrefs) {
            final String key = getEngineSearchCountKey(engineWhereStr);
            out.put(engineWhereStr, prefs.getInt(key, 0));
            editor.remove(key);
        }
        editor.remove(PREF_SEARCH_KEYSET)
                .apply();
        return out;
    }

    /**
     * @param engineWhereStr string of the form "engine.where"
     * @return the key for the engines' search counts in shared preferences
     */
    @VisibleForTesting static String getEngineSearchCountKey(final String engineWhereStr) {
        return PREF_SEARCH_PREFIX + engineWhereStr;
    }
}
