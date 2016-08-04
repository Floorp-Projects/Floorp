/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.util;

import java.util.HashSet;
import java.util.Set;

import org.json.JSONArray;
import org.json.JSONException;
import org.mozilla.gecko.AppConstants.Versions;

import android.content.SharedPreferences;
import android.util.Log;


public class PrefUtils {
    private static final String LOGTAG = "GeckoPrefUtils";

    // Cross version compatible way to get a string set from a pref
    public static Set<String> getStringSet(final SharedPreferences prefs,
                                           final String key,
                                           final Set<String> defaultVal) {
        if (!prefs.contains(key)) {
            return defaultVal;
        }

        // If this is Android version >= 11, try to use a Set<String>.
        try {
            return prefs.getStringSet(key, new HashSet<String>());
        } catch (ClassCastException ex) {
            // A ClassCastException means we've upgraded from a pre-v11 Android to a new one
            final Set<String> val = getFromJSON(prefs, key);
            SharedPreferences.Editor edit = prefs.edit();
            putStringSet(edit, key, val).apply();
            return val;
        }
    }

    private static Set<String> getFromJSON(SharedPreferences prefs, String key) {
        try {
            final String val = prefs.getString(key, "[]");
            return JSONUtils.parseStringSet(new JSONArray(val));
        } catch (JSONException ex) {
            Log.i(LOGTAG, "Unable to parse JSON", ex);
        }

        return new HashSet<String>();
    }

    /**
     * Cross version compatible way to save a set of strings.
     * <p>
     * This method <b>does not commit</b> any transaction. It is up to callers
     * to commit.
     *
     * @param editor to write to.
     * @param key to write.
     * @param vals comprising string set.
     * @return
     */
    public static SharedPreferences.Editor putStringSet(final SharedPreferences.Editor editor,
                                    final String key,
                                    final Set<String> vals) {
        editor.putStringSet(key, vals);
        return editor;
    }
}
