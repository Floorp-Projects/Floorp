/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.search

import android.content.Context
import android.content.SharedPreferences
import android.util.AttributeSet
import androidx.preference.Preference
import mozilla.components.browser.state.state.selectedOrDefaultSearchEngine
import org.mozilla.focus.R
import org.mozilla.focus.ext.components

/**
 * Preference for setting the default search engine.
 */
class SearchEnginePreference : Preference, SharedPreferences.OnSharedPreferenceChangeListener {
    internal val context: Context

    constructor(context: Context, attrs: AttributeSet) : super(context, attrs) {
        this.context = context
    }

    constructor(context: Context, attrs: AttributeSet, defStyleAttr: Int) : super(context, attrs, defStyleAttr) {
        this.context = context
    }

    override fun onAttached() {
        summary = defaultSearchEngineName
        preferenceManager.sharedPreferences.registerOnSharedPreferenceChangeListener(this)
        super.onAttached()
    }

    override fun onPrepareForRemoval() {
        preferenceManager.sharedPreferences.unregisterOnSharedPreferenceChangeListener(this)
        super.onPrepareForRemoval()
    }

    override fun onSharedPreferenceChanged(sharedPreferences: SharedPreferences, key: String) {
        if (key == context.resources.getString(R.string.pref_key_search_engine)) {
            summary = defaultSearchEngineName
        }
    }

    private val defaultSearchEngineName: String
        get() = context.components.store.state.search.selectedOrDefaultSearchEngine?.name ?: ""
}
