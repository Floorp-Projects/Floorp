/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.settings;

import android.content.Intent;
import android.os.Bundle;
import android.preference.Preference;
import android.preference.PreferenceFragment;
import android.preference.PreferenceGroup;
import android.preference.PreferenceScreen;
import android.support.annotation.Nullable;

import org.mozilla.focus.R;
import org.mozilla.focus.activity.HelpActivity;
import org.mozilla.focus.activity.InfoActivity;
import org.mozilla.focus.widget.DefaultBrowserPreference;

public class SettingsFragment extends PreferenceFragment {
    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        addPreferencesFromResource(R.xml.settings);
    }

    @Override
    public boolean onPreferenceTreeClick(PreferenceScreen preferenceScreen, Preference preference) {
        // We could use preference keys (instead of titles) here, but the only places those would
        // be used are in the menu XML, and here - so we might as well use the already
        // existing preference titles:
        if (preference.getTitle().equals(getResources().getString(R.string.menu_about))) {
            final Intent intent = InfoActivity.getAboutIntent(getActivity());
            startActivity(intent);
        } else if (preference.getTitle().equals(getResources().getString(R.string.menu_help))) {
            final Intent intent = new Intent(getActivity(), HelpActivity.class);
            startActivity(intent);
        } else if (preference.getTitle().equals(getResources().getString(R.string.menu_rights))) {
            final Intent intent = InfoActivity.getRightsIntent(getActivity());
            startActivity(intent);
        }

        return super.onPreferenceTreeClick(preferenceScreen, preference);
    }
}
