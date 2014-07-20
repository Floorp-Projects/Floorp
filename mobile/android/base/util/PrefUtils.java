/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.util;

import android.content.SharedPreferences;
import android.os.Build;
import android.util.Log;

import java.util.HashSet;
import java.util.Set;

import org.json.JSONArray;
import org.json.JSONException;


public class PrefUtils {
    private static final String LOGTAG = "GeckoPrefUtils";

    // Cross version compatible way to get a string set from a pref
    public static Set<String> getStringSet(final SharedPreferences prefs,
                                           final String key,
                                           final Set<String> defaultVal) {
        if (!prefs.contains(key)) {
            return defaultVal;
        }

        if (Build.VERSION.SDK_INT < 11) {
            return getFromJSON(prefs, key);
        }

        // If this is Android version >= 11, try to use a Set<String>.
        try {
            return prefs.getStringSet(key, new HashSet<String>());
        } catch(ClassCastException ex) {
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
        } catch(JSONException ex) {
            Log.i(LOGTAG, "Unable to parse JSON", ex);
        }

        return new HashSet<String>();
    }

    // Cross version compatible way to save a string set to a pref.
    // NOTE: The editor that is passed in will not commit the transaction for you. It is up to callers to commit
    //       when they are done with any other changes to the database.
    public static SharedPreferences.Editor putStringSet(final SharedPreferences.Editor edit,
                                    final String key,
                                    final Set<String> vals) {
        if (Build.VERSION.SDK_INT < 11) {
            final JSONArray json = new JSONArray(vals);
            edit.putString(key, json.toString()).apply();
        } else {
            edit.putStringSet(key, vals).apply();
        }

        return edit;
    }
}
