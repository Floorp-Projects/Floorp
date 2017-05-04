/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.utils;

import android.content.Context;
import android.content.SharedPreferences;
import android.content.res.Resources;
import android.preference.PreferenceManager;
import android.support.annotation.Nullable;

import org.mozilla.focus.BuildConfig;
import org.mozilla.focus.R;
import org.mozilla.focus.fragment.FirstrunFragment;
import org.mozilla.focus.search.SearchEngine;

/**
 * A simple wrapper for SharedPreferences that makes reading preference a little bit easier.
 */
public class Settings {
    private static Settings instance;

    public synchronized static Settings getInstance(Context context) {
        if (instance == null) {
            instance = new Settings(context.getApplicationContext());
        }
        return instance;
    }

    private final SharedPreferences preferences;
    private final Resources resources;

    private Settings(Context context) {
        preferences = PreferenceManager.getDefaultSharedPreferences(context);
        resources = context.getResources();
    }

    public boolean shouldBlockImages() {
        // Not shipping in v1 (#188)
        return preferences.getBoolean(
                resources.getString(R.string.pref_key_performance_block_images),
                false);
    }

    public boolean shouldShowFirstrun() {
        return true;
        //return !preferences.getBoolean(FirstrunFragment.FIRSTRUN_PREF, false);
    }

    public boolean shouldUseSecureMode() {
        // Always allow screenshots in debug builds - it's really hard to get UX feedback
        // without screenshots.
        if (AppConstants.isDevBuild()) {
            return false;
        }

        return preferences.getBoolean(getPreferenceKey(R.string.pref_key_secure), true);
    }

    @Nullable
    public String getDefaultSearchEngineName() {
        return preferences.getString(getPreferenceKey(R.string.pref_key_search_engine), null);
    }

    public void setDefaultSearchEngine(SearchEngine searchEngine) {
        preferences.edit()
                .putString(getPreferenceKey(R.string.pref_key_search_engine), searchEngine.getName())
                .apply();
    }

    /* package */ String getPreferenceKey(int resourceId) {
        return resources.getString(resourceId);
    }
}
