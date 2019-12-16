/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.searchengine

import android.content.Context
import androidx.annotation.VisibleForTesting
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.async
import kotlinx.coroutines.launch
import kotlinx.coroutines.runBlocking
import mozilla.components.browser.search.SearchEngine
import mozilla.components.browser.search.provider.AssetsSearchEngineProvider
import mozilla.components.browser.search.provider.SearchEngineList
import mozilla.components.browser.search.provider.SearchEngineProvider
import mozilla.components.browser.search.provider.filter.SearchEngineFilter
import mozilla.components.browser.search.provider.localization.LocaleSearchLocalizationProvider
import org.mozilla.fenix.ext.settings
import java.util.Locale

@SuppressWarnings("TooManyFunctions")
open class FenixSearchEngineProvider(
    private val context: Context
) : SearchEngineProvider, CoroutineScope by CoroutineScope(Job() + Dispatchers.IO) {
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    open val baseSearchEngines = async {
        AssetsSearchEngineProvider(LocaleSearchLocalizationProvider()).loadSearchEngines(context)
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    open val bundledSearchEngines = async {
        val defaultEngineIdentifiers = baseSearchEngines.await().list.map { it.identifier }.toSet()
        AssetsSearchEngineProvider(
            LocaleSearchLocalizationProvider(),
            filters = listOf(object : SearchEngineFilter {
                override fun filter(context: Context, searchEngine: SearchEngine): Boolean {
                    return BUNDLED_SEARCH_ENGINES.contains(searchEngine.identifier) &&
                            !defaultEngineIdentifiers.contains(searchEngine.identifier)
                }
            }),
            additionalIdentifiers = BUNDLED_SEARCH_ENGINES
        ).loadSearchEngines(context)
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    open var customSearchEngines = async {
        CustomSearchEngineProvider().loadSearchEngines(context)
    }

    private var loadedSearchEngines = refreshAsync()

    fun getDefaultEngine(context: Context): SearchEngine {
        val engines = installedSearchEngines(context)
        val selectedName = context.settings().defaultSearchEngineName

        return engines.list.find { it.name == selectedName } ?: engines.default ?: engines.list.first()
    }

    /**
     * @return a list of all SearchEngines that are currently active. These are the engines that
     * are readily available throughout the app.
     */
    fun installedSearchEngines(context: Context): SearchEngineList = runBlocking {
        val engineList = loadedSearchEngines.await()
        val installedIdentifiers = installedSearchEngineIdentifiers(context)

        engineList.copy(
            list = engineList.list.filter {
                installedIdentifiers.contains(it.identifier)
            }.sortedBy { it.name.toLowerCase(Locale.getDefault()) },
            default = engineList.default?.let {
                if (installedIdentifiers.contains(it.identifier)) {
                    it
                } else {
                    null
                }
            }
        )
    }

    fun allSearchEngineIdentifiers() = runBlocking {
        loadedSearchEngines.await().list.map { it.identifier }
    }

    fun uninstalledSearchEngines(context: Context): SearchEngineList = runBlocking {
        val engineList = loadedSearchEngines.await()
        val installedIdentifiers = installedSearchEngineIdentifiers(context)

        engineList.copy(list = engineList.list.filterNot { installedIdentifiers.contains(it.identifier) })
    }

    override suspend fun loadSearchEngines(context: Context): SearchEngineList {
        return installedSearchEngines(context)
    }

    fun installSearchEngine(context: Context, searchEngine: SearchEngine) = runBlocking {
        val installedIdentifiers = installedSearchEngineIdentifiers(context).toMutableSet()
        installedIdentifiers.add(searchEngine.identifier)
        prefs(context).edit().putStringSet(INSTALLED_ENGINES_KEY, installedIdentifiers).apply()
    }

    fun uninstallSearchEngine(context: Context, searchEngine: SearchEngine) = runBlocking {
        val isCustom = CustomSearchEngineStore.isCustomSearchEngine(context, searchEngine.identifier)

        if (isCustom) {
            CustomSearchEngineStore.removeSearchEngine(context, searchEngine.identifier)
        } else {
            val installedIdentifiers = installedSearchEngineIdentifiers(context).toMutableSet()
            installedIdentifiers.remove(searchEngine.identifier)
            prefs(context).edit().putStringSet(INSTALLED_ENGINES_KEY, installedIdentifiers).apply()
        }

        reload()
    }

    fun reload() {
        launch {
            customSearchEngines = async { CustomSearchEngineProvider().loadSearchEngines(context) }
            loadedSearchEngines = refreshAsync()
        }
    }

    private fun refreshAsync() = async {
        val engineList = baseSearchEngines.await()
        val bundledList = bundledSearchEngines.await().list
        val customList = customSearchEngines.await().list

        engineList.copy(list = engineList.list + bundledList + customList)
    }

    private fun prefs(context: Context) = context.getSharedPreferences(
        PREF_FILE,
        Context.MODE_PRIVATE
    )

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    suspend fun installedSearchEngineIdentifiers(context: Context): Set<String> {
        val prefs = prefs(context)

        if (!prefs.contains(INSTALLED_ENGINES_KEY)) {
            val defaultSet = baseSearchEngines.await()
                .list
                .map { it.identifier }
                .toSet()

            prefs.edit().putStringSet(INSTALLED_ENGINES_KEY, defaultSet).apply()
        }

        val installedIdentifiers = prefs(context).getStringSet(INSTALLED_ENGINES_KEY, setOf()) ?: setOf()
        val customEngineIdentifiers = customSearchEngines.await().list.map { it.identifier }.toSet()
        return installedIdentifiers + customEngineIdentifiers
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    companion object {
        val BUNDLED_SEARCH_ENGINES = listOf("reddit", "youtube")
        const val PREF_FILE = "fenix-search-engine-provider"
        const val INSTALLED_ENGINES_KEY = "fenix-installed-search-engines"
    }
}
