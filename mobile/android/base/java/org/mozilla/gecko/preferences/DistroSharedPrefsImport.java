/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.preferences;

import org.mozilla.gecko.GeckoSharedPrefs;
import org.mozilla.gecko.distribution.Distribution;

import android.content.Context;
import android.content.SharedPreferences;
import android.util.Log;
import org.json.JSONException;
import org.json.JSONObject;

import java.util.Iterator;

public class DistroSharedPrefsImport {

    public static final String LOGTAG = DistroSharedPrefsImport.class.getSimpleName();

    public static void importPreferences(final Context context, final Distribution distribution) {
        if (distribution == null) {
            return;
        }

        final JSONObject preferences = distribution.getAndroidPreferences();
        if (preferences.length() == 0) {
            return;
        }

        final Iterator<?> keys = preferences.keys();
        final SharedPreferences.Editor sharedPreferences = GeckoSharedPrefs.forProfile(context).edit();

        while (keys.hasNext()) {
            final String key = (String) keys.next();
            final Object value;
            try {
                value = preferences.get(key);
            } catch (JSONException e) {
                Log.e(LOGTAG, "Unable to completely process Android Preferences JSON.", e);
                continue;
            }

            // We currently don't support Float preferences.
            if (value instanceof String) {
                sharedPreferences.putString(GeckoPreferences.NON_PREF_PREFIX + key, (String) value);
            } else if (value instanceof Boolean) {
                sharedPreferences.putBoolean(GeckoPreferences.NON_PREF_PREFIX + key, (boolean) value);
            } else if (value instanceof Integer) {
                sharedPreferences.putInt(GeckoPreferences.NON_PREF_PREFIX + key, (int) value);
            } else if (value instanceof Long) {
                sharedPreferences.putLong(GeckoPreferences.NON_PREF_PREFIX + key, (long) value);
            } else {
                Log.d(LOGTAG, "Unknown preference value type whilst importing android preferences from distro file.");
            }
        }
        sharedPreferences.apply();
    }
}
