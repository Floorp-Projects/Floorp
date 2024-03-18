/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.search

import android.content.Context
import android.content.SharedPreferences
import mozilla.components.browser.state.search.SearchEngine
import mozilla.components.feature.search.ext.parseLegacySearchEngine
import mozilla.components.feature.search.middleware.SearchMiddleware
import org.mozilla.focus.ext.settings
import org.xmlpull.v1.XmlPullParserException
import java.io.BufferedInputStream
import java.io.IOException

private const val PREF_FILE_SEARCH_ENGINES = "custom-search-engines"
private const val PREF_KEY_MIGRATED = "pref_search_migrated"
private const val PREF_KEY_CUSTOM_SEARCH_ENGINES = "pref_custom_search_engines"

/**
 * Helper class to migrate the search related data in Focus to the "Android Components" implementation.
 */
class SearchMigration(
    private val context: Context,
) : SearchMiddleware.Migration {

    override fun getValuesToMigrate(): SearchMiddleware.Migration.MigrationValues? {
        val preferences = context.getSharedPreferences(PREF_FILE_SEARCH_ENGINES, Context.MODE_PRIVATE)
        if (preferences.getBoolean(PREF_KEY_MIGRATED, false)) {
            return null
        }

        @Suppress("DEPRECATION")
        val values = SearchMiddleware.Migration.MigrationValues(
            customSearchEngines = loadCustomSearchEngines(preferences),
            defaultSearchEngineName = context.settings.defaultSearchEngineName,
        )

        preferences.edit()
            .putBoolean(PREF_KEY_MIGRATED, true)
            .apply()

        return values
    }

    private fun loadCustomSearchEngines(
        preferences: SharedPreferences,
    ): List<SearchEngine> {
        val engines = preferences.getStringSet(PREF_KEY_CUSTOM_SEARCH_ENGINES, emptySet())!!

        return engines.mapNotNull { engine ->
            val engineInputStream = preferences.getString(engine, "")!!
                .byteInputStream()
                .buffered()

            loadSafely(engine, engineInputStream)
        }
    }
}

@Suppress("SwallowedException", "DEPRECATION")
private fun loadSafely(id: String, stream: BufferedInputStream?): SearchEngine? {
    return try {
        stream?.let { parseLegacySearchEngine(id, it) }
    } catch (e: IOException) {
        null
    } catch (e: XmlPullParserException) {
        null
    }
}
