/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.search;

import android.content.Context;
import android.content.SharedPreferences;
import android.preference.Preference;
import android.util.AttributeSet;

import org.mozilla.focus.R;
import org.mozilla.focus.utils.Settings;

/**
 * Preference for setting the default search engine.
 */
public class SearchEnginePreference extends Preference implements SharedPreferences.OnSharedPreferenceChangeListener {
    final Context context;

    public SearchEnginePreference(Context context, AttributeSet attrs) {
        super(context, attrs);
        this.context = context;
    }

    public SearchEnginePreference(Context context, AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
        this.context = context;
    }

    @Override
    protected void onAttachedToActivity() {
        setTitle(SearchEngineManager.getInstance().getDefaultSearchEngine(getContext()).getName());
        getPreferenceManager().getSharedPreferences().registerOnSharedPreferenceChangeListener(this);
        super.onAttachedToActivity();
    }

    @Override
    protected void onPrepareForRemoval() {
        getPreferenceManager().getSharedPreferences().unregisterOnSharedPreferenceChangeListener(this);
        super.onPrepareForRemoval();
    }

    @Override
    public void onSharedPreferenceChanged(SharedPreferences sharedPreferences, String key) {
        if (key.equals(context.getResources().getString(R.string.pref_key_search_engine))) {
            setTitle(Settings.getInstance(context).getDefaultSearchEngineName());
        }
    }
}
