/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.settings;

import android.os.Bundle;
import android.preference.Preference;
import android.preference.PreferenceFragment;
import android.preference.PreferenceGroup;
import android.support.annotation.Nullable;

import org.mozilla.focus.R;
import org.mozilla.focus.widget.DefaultBrowserPreference;

public class SettingsFragment extends PreferenceFragment {
    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        addPreferencesFromResource(R.xml.settings);

        setupPreferences(getPreferenceScreen());
    }

    private void setupPreferences(PreferenceGroup group) {
        for (int i = 0; i < group.getPreferenceCount(); i++) {
            final Preference preference = group.getPreference(i);

            if (preference instanceof PreferenceGroup) {
                setupPreferences((PreferenceGroup) preference);
            }

            if (preference instanceof DefaultBrowserPreference
                    && !((DefaultBrowserPreference) preference).shouldBeVisible()) {
                group.removePreference(preference);
                i--;
            }
        }
    }
}
