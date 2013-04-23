/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import java.util.ArrayList;

import android.preference.Preference;
import android.preference.PreferenceCategory;
import android.preference.PreferenceFragment;
import android.preference.PreferenceScreen;
import android.os.Bundle;
import android.util.Log;

/* A simple implementation of PreferenceFragment for large screen devices
 * This will strip category headers (so that they aren't shown to the user twice)
 * as well as initializing Gecko prefs when a fragment is shown.
*/
public class GeckoPreferenceFragment extends PreferenceFragment {

    private static final String LOGTAG = "GeckoPreferenceFragment";

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        String resource = getArguments().getString("resource");
        int res = getActivity().getResources().getIdentifier(resource,
                                                             "xml",
                                                             getActivity().getPackageName());
        addPreferencesFromResource(res);

        /* This is only hit when we're using the headers (i.e. on large screen devices).
           Strip the first category it isn't shown twice */
        PreferenceScreen screen = stripCategories(getPreferenceScreen());
        setPreferenceScreen(screen);
        ((GeckoPreferences)getActivity()).setupPreferences(screen);
    }

    private PreferenceScreen stripCategories(PreferenceScreen preferenceScreen) {
        PreferenceScreen newScreen = getPreferenceManager().createPreferenceScreen(preferenceScreen.getContext());
        int order = 0;
        if (preferenceScreen.getPreferenceCount() > 0 && preferenceScreen.getPreference(0) instanceof PreferenceCategory) {
            PreferenceCategory cat = (PreferenceCategory) preferenceScreen.getPreference(0);
            for (int i = 0; i < cat.getPreferenceCount(); i++) {
                Preference pref = cat.getPreference(i);
                pref.setOrder(order++);
                newScreen.addPreference(pref);
            }
        }

        for (int i = 1; i < preferenceScreen.getPreferenceCount(); i++) {
            Preference pref = preferenceScreen.getPreference(i);
            pref.setOrder(order++);
            newScreen.addPreference(pref);
        }

        return newScreen;
    }
}
