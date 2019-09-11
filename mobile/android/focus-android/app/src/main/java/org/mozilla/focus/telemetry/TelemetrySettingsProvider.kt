/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("DEPRECATION")

package org.mozilla.focus.telemetry

import android.content.Context
import org.mozilla.focus.R
import org.mozilla.focus.ext.components
import org.mozilla.focus.search.CustomSearchEngineStore
import org.mozilla.focus.utils.Browsers
import org.mozilla.focus.utils.Settings
import org.mozilla.focus.utils.app
import org.mozilla.telemetry.TelemetryHolder
import org.mozilla.telemetry.measurement.SettingsMeasurement

/**
 * SharedPreferenceSettingsProvider implementation that additionally injects settings value for
 * runtime preferences like "default browser" and "search engine".
 */
internal class TelemetrySettingsProvider(
    private val context: Context
) : SettingsMeasurement.SharedPreferenceSettingsProvider() {
    private val prefKeyDefaultBrowser: String
    private val prefKeySearchEngine: String
    private val prefKeyFretboardBucketNumber: String

    init {
        val resources = context.resources
        prefKeyDefaultBrowser = resources.getString(R.string.pref_key_default_browser)
        prefKeySearchEngine = resources.getString(R.string.pref_key_search_engine)
        prefKeyFretboardBucketNumber = resources.getString(R.string.pref_key_fretboard_bucket_number)
    }

    override fun containsKey(key: String): Boolean = when (key) {
        // Not actually a setting - but we want to report this like a setting.
        prefKeyDefaultBrowser -> true

        // We always want to report the current search engine - even if it's not in settings yet.
        prefKeySearchEngine -> true

        // Not actually a setting - but we want to report this like a setting.
        prefKeyFretboardBucketNumber -> true

        else -> super.containsKey(key)
    }

    override fun getValue(key: String): Any {
        return when (key) {
            prefKeyDefaultBrowser -> {
                // The default browser is not actually a setting. We determine if we are the
                // default and then inject this into telemetry.
                val context = TelemetryHolder.get().configuration.context
                val browsers = Browsers(context, Browsers.TRADITIONAL_BROWSER_URL)
                java.lang.Boolean.toString(browsers.isDefaultBrowser(context))
            }
            prefKeySearchEngine -> {
                var value: Any? = super.getValue(key)
                if (value == null) {
                    // If the user has never selected a search engine then this value is null.
                    // However we still want to report the current search engine of the user.
                    // Therefore we inject this value at runtime.
                    value = context.components.searchEngineManager.getDefaultSearchEngine(
                            context,
                            Settings.getInstance(context).defaultSearchEngineName
                    ).name
                } else if (CustomSearchEngineStore.isCustomSearchEngine((value as String?)!!, context)) {
                    // Don't collect possibly sensitive info for custom search engines, send "custom" instead
                    value = CustomSearchEngineStore.ENGINE_TYPE_CUSTOM
                }
                value
            }
            prefKeyFretboardBucketNumber -> {
                context.app.fretboard.getUserBucket(context)
            }
            else -> super.getValue(key)
        }
    }
}
