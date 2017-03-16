/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.settings;

import android.content.Intent;
import android.os.Bundle;
import android.preference.Preference;
import android.preference.PreferenceFragment;
import android.preference.PreferenceScreen;
import android.support.annotation.Nullable;

import org.mozilla.focus.R;
import org.mozilla.focus.activity.HelpActivity;
import org.mozilla.focus.activity.InfoActivity;

public class SettingsFragment extends PreferenceFragment {
    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        addPreferencesFromResource(R.xml.settings);
    }

    @Override
    public boolean onPreferenceTreeClick(PreferenceScreen preferenceScreen, Preference preference) {
        if (preference.getKey().equals(getResources().getString(R.string.pref_key_about))) {
            final Intent intent = InfoActivity.getAboutIntent(getActivity());
            startActivity(intent);
        } else if (preference.getKey().equals(getResources().getString(R.string.pref_key_help))) {
            final Intent intent = new Intent(getActivity(), HelpActivity.class);
            startActivity(intent);
        } else if (preference.getKey().equals(getResources().getString(R.string.pref_key_rights))) {
            final Intent intent = InfoActivity.getRightsIntent(getActivity());
            startActivity(intent);
        }

        return super.onPreferenceTreeClick(preferenceScreen, preference);
    }
}
