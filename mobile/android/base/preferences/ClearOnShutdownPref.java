/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.preferences;

import java.util.HashSet;
import java.util.Set;

import org.mozilla.gecko.GeckoSharedPrefs;
import org.mozilla.gecko.util.PrefUtils;

import android.content.Context;
import android.content.SharedPreferences;
import android.preference.Preference;

public class ClearOnShutdownPref implements GeckoPreferences.PrefHandler {
    public static final String PREF = GeckoPreferences.NON_PREF_PREFIX + "history.clear_on_exit";

    @Override
    public void setupPref(Context context, Preference pref) {
        // The pref is initialized asynchronously. Read the pref explicitly
        // here to make sure we have the data.
        final SharedPreferences prefs = GeckoSharedPrefs.forProfile(context);
        final Set<String> clearItems = PrefUtils.getStringSet(prefs, PREF, new HashSet<String>());
        ((ListCheckboxPreference) pref).setChecked(clearItems.size() > 0);
    }

    @Override
    @SuppressWarnings("unchecked")
    public void onChange(Context context, Preference pref, Object newValue) {
        final Set<String> vals = (Set<String>) newValue;
        ((ListCheckboxPreference) pref).setChecked(vals.size() > 0);
    }
}
