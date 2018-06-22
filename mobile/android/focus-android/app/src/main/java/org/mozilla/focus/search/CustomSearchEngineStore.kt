/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.search

import android.content.Context
import android.util.Log
import mozilla.components.browser.search.SearchEngine
import mozilla.components.browser.search.SearchEngineParser
import org.mozilla.focus.shortcut.IconGenerator
import org.xmlpull.v1.XmlPullParserException
import java.io.IOException

object CustomSearchEngineStore {
    fun isSearchEngineNameUnique(context: Context, engineName: String): Boolean {
        val sharedPreferences = context.getSharedPreferences(PREF_FILE_SEARCH_ENGINES,
                Context.MODE_PRIVATE)

        return !sharedPreferences.contains(engineName)
    }

    fun restoreDefaultSearchEngines(context: Context) {
        pref(context)
                .edit()
                .remove(PREF_KEY_HIDDEN_DEFAULT_ENGINES)
                .apply()
    }

    fun hasAllDefaultSearchEngines(context: Context): Boolean {
        return !pref(context)
                .contains(PREF_KEY_HIDDEN_DEFAULT_ENGINES)
    }

    fun addSearchEngine(context: Context, engineName: String, searchQuery: String): Boolean {
        // This is not for a homescreen shortcut so we don't want an adaptive launcher icon.
        val iconBitmap = IconGenerator.generateSearchEngineIcon(context)
        val searchEngineXml = SearchEngineWriter.buildSearchEngineXML(engineName, searchQuery, iconBitmap)
                ?: return false
        val existingEngines = pref(context).getStringSet(PREF_KEY_CUSTOM_SEARCH_ENGINES, emptySet())
        val newEngines = LinkedHashSet<String>()
        newEngines.addAll(existingEngines!!)
        newEngines.add(engineName)

        pref(context)
                .edit()
                .putInt(PREF_KEY_CUSTOM_SEARCH_VERSION, CUSTOM_SEARCH_VERSION)
                .putStringSet(PREF_KEY_CUSTOM_SEARCH_ENGINES, newEngines)
                .putString(engineName, searchEngineXml)
                .apply()

        return true
    }

    fun removeSearchEngines(context: Context, engineIdsToRemove: MutableSet<String>) {
        // Check custom engines first.
        val customEngines = pref(context).getStringSet(PREF_KEY_CUSTOM_SEARCH_ENGINES, emptySet())
        val remainingCustomEngines = LinkedHashSet<String>()
        val enginesEditor = pref(context).edit()

        for (engineId in customEngines!!) {
            if (engineIdsToRemove.contains(engineId)) {
                enginesEditor.remove(engineId)
                // Handled engine removal.
                engineIdsToRemove.remove(engineId)
            } else {
                remainingCustomEngines.add(engineId)
            }
        }
        enginesEditor.putStringSet(PREF_KEY_CUSTOM_SEARCH_ENGINES, remainingCustomEngines)

        // Everything else must be a default engine.
        val hiddenDefaultEngines = pref(context).getStringSet(PREF_KEY_HIDDEN_DEFAULT_ENGINES, emptySet())
        engineIdsToRemove.addAll(hiddenDefaultEngines!!)
        enginesEditor.putStringSet(PREF_KEY_HIDDEN_DEFAULT_ENGINES, engineIdsToRemove)

        enginesEditor.apply()
    }

    fun getRemovedSearchEngines(context: Context): Set<String> =
            pref(context).getStringSet(PREF_KEY_HIDDEN_DEFAULT_ENGINES, emptySet())

    fun isCustomSearchEngine(engineId: String, context: Context): Boolean {
        for (e in loadCustomSearchEngines(context)) {
            if (e.identifier == engineId) {
                return true
            }
        }
        return false
    }

    fun loadCustomSearchEngines(context: Context): List<SearchEngine> {
        val customEngines = mutableListOf<SearchEngine>()
        val parser = SearchEngineParser()
        val prefs = context.getSharedPreferences(PREF_FILE_SEARCH_ENGINES, Context.MODE_PRIVATE)
        val engines = prefs.getStringSet(PREF_KEY_CUSTOM_SEARCH_ENGINES, emptySet())

        try {
            for (engine in engines!!) {
                val engineInputStream = prefs.getString(engine, "").byteInputStream().buffered()
                // Search engine identifier is the search engine name.
                customEngines.add(parser.load(engine, engineInputStream))
            }
        } catch (e: IOException) {
            Log.e(LOG_TAG, "IOException while loading custom search engines", e)
        } catch (e: XmlPullParserException) {
            Log.e(LOG_TAG, "Couldn't load custom search engines", e)
        }

        return customEngines
    }

    private fun pref(context: Context) =
            context.getSharedPreferences(PREF_FILE_SEARCH_ENGINES, Context.MODE_PRIVATE)

    const val ENGINE_TYPE_CUSTOM = "custom"
    const val ENGINE_TYPE_BUNDLED = "bundled"

    private const val LOG_TAG = "CustomSearchEngineStore"
    private const val PREF_KEY_CUSTOM_SEARCH_ENGINES = "pref_custom_search_engines"
    private const val PREF_KEY_HIDDEN_DEFAULT_ENGINES = "hidden_default_engines"
    private const val PREF_KEY_CUSTOM_SEARCH_VERSION = "pref_custom_search_version"
    private const val CUSTOM_SEARCH_VERSION = 1
    private const val PREF_FILE_SEARCH_ENGINES = "custom-search-engines"
}
