/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("DEPRECATION")

package org.mozilla.focus.telemetry

import android.content.Context
import mozilla.components.browser.state.search.SearchEngine
import mozilla.components.browser.state.state.selectedOrDefaultSearchEngine
import mozilla.components.support.utils.Browsers
import org.mozilla.focus.R
import org.mozilla.focus.ext.components
import org.mozilla.telemetry.TelemetryHolder
import org.mozilla.telemetry.measurement.SettingsMeasurement

/**
 * SharedPreferenceSettingsProvider implementation that additionally injects settings value for
 * runtime preferences like "default browser" and "search engine".
 */
internal class TelemetrySettingsProvider(
    private val context: Context,
) : SettingsMeasurement.SharedPreferenceSettingsProvider() {
    private val prefKeyDefaultBrowser: String
    private val prefKeySearchEngine: String

    init {
        val resources = context.resources
        prefKeyDefaultBrowser = resources.getString(R.string.pref_key_default_browser)
        prefKeySearchEngine = resources.getString(R.string.pref_key_search_engine)
    }

    override fun containsKey(key: String): Boolean = when (key) {
        // Not actually a setting - but we want to report this like a setting.
        prefKeyDefaultBrowser -> true

        // We always want to report the current search engine - even if it's not in settings yet.
        prefKeySearchEngine -> true

        else -> super.containsKey(key)
    }

    override fun getValue(key: String): Any {
        return when (key) {
            prefKeyDefaultBrowser -> {
                // The default browser is not actually a setting. We determine if we are the
                // default and then inject this into telemetry.
                val context = TelemetryHolder.get().configuration.context
                val browsers = Browsers.all(context)
                browsers.isDefaultBrowser.toString()
            }
            prefKeySearchEngine -> {
                // The default search engine is no longer saved using this pref. But we will continue
                // to report it here until we switch to our new telemetry system (glean).
                val searchEngine = context.components.store.state.search.selectedOrDefaultSearchEngine
                if (searchEngine?.type == SearchEngine.Type.CUSTOM) {
                    "custom"
                } else {
                    searchEngine?.name ?: "<none>"
                }
            }
            else -> super.getValue(key)
        }
    }
}
