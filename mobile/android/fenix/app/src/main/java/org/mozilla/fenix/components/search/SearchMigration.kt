/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.search

import android.content.Context
import android.content.SharedPreferences
import mozilla.components.browser.search.SearchEngineParser
import mozilla.components.browser.state.search.SearchEngine
import mozilla.components.feature.search.ext.migrate
import mozilla.components.feature.search.middleware.SearchMiddleware
import org.mozilla.fenix.ext.components
import org.xmlpull.v1.XmlPullParserException
import java.io.BufferedInputStream
import java.io.IOException

private const val PREF_FILE_SEARCH_ENGINES = "custom-search-engines"

private const val PREF_KEY_CUSTOM_SEARCH_ENGINES = "pref_custom_search_engines"
private const val PREF_KEY_MIGRATED = "pref_search_migrated"

/**
 * Helper class to migrate the search related data in Fenix to the "Android Components" implementation.
 */
internal class SearchMigration(
    private val context: Context
) : SearchMiddleware.Migration {

    override fun getValuesToMigrate(): SearchMiddleware.Migration.MigrationValues? {
        val preferences = context.getSharedPreferences(PREF_FILE_SEARCH_ENGINES, Context.MODE_PRIVATE)
        if (preferences.getBoolean(PREF_KEY_MIGRATED, false)) {
            return null
        }

        val values = SearchMiddleware.Migration.MigrationValues(
            customSearchEngines = loadCustomSearchEngines(preferences),
            defaultSearchEngineName = context.components.settings.defaultSearchEngineName
        )

        preferences.edit()
            .putBoolean(PREF_KEY_MIGRATED, true)
            .apply()

        return values
    }

    private fun loadCustomSearchEngines(
        preferences: SharedPreferences
    ): List<SearchEngine> {
        val ids = preferences.getStringSet(PREF_KEY_CUSTOM_SEARCH_ENGINES, emptySet()) ?: emptySet()

        val parser = SearchEngineParser()

        return ids.mapNotNull { id ->
            val xml = preferences.getString(id, null)
            parser.loadSafely(id, xml?.byteInputStream()?.buffered())
        }
    }
}

private fun SearchEngineParser.loadSafely(id: String, stream: BufferedInputStream?): SearchEngine? {
    return try {
        stream?.let { load(id, it).migrate() }
    } catch (e: IOException) {
        null
    } catch (e: XmlPullParserException) {
        null
    }
}
