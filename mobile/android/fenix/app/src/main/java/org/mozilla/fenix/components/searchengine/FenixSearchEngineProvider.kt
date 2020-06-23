/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.searchengine

import android.content.Context
import androidx.annotation.VisibleForTesting
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Deferred
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
import mozilla.components.browser.search.provider.localization.SearchLocalizationProvider
import mozilla.components.service.location.LocationService
import mozilla.components.service.location.MozillaLocationService
import mozilla.components.service.location.search.RegionSearchLocalizationProvider
import org.mozilla.fenix.BuildConfig
import org.mozilla.fenix.Config
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.settings
import java.util.Locale

@SuppressWarnings("TooManyFunctions")
open class FenixSearchEngineProvider(
    private val context: Context
) : SearchEngineProvider, CoroutineScope by CoroutineScope(Job() + Dispatchers.IO) {
    private val locationService: LocationService = if (Config.channel.isDebug) {
        LocationService.dummy()
    } else {
        MozillaLocationService(
            context,
            context.components.core.client,
            BuildConfig.MLS_TOKEN
        )
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    open val localizationProvider: SearchLocalizationProvider =
        RegionSearchLocalizationProvider(locationService)

    open var baseSearchEngines = async {
        AssetsSearchEngineProvider(localizationProvider).loadSearchEngines(context)
    }

    // https://github.com/mozilla-mobile/fenix/issues/9935
    // Adds a Locale search engine provider as a fallback in case the MLS lookup takes longer
    // than the time it takes for a user to try to search.
    private val fallBackEngines = async {
        AssetsSearchEngineProvider(LocaleSearchLocalizationProvider()).loadSearchEngines(context)
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    open val bundledSearchEngines = async {
        val defaultEngineIdentifiers = baseSearchEngines.await().list.map { it.identifier }.toSet()
        AssetsSearchEngineProvider(
            localizationProvider,
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

    // https://github.com/mozilla-mobile/fenix/issues/9935
    // Create new getter that will return the fallback SearchEngineList if
    // the main one hasn't completed yet
    private val searchEngines: Deferred<SearchEngineList>
        get() =
            if (loadedSearchEngines.isCompleted) {
                loadedSearchEngines
            } else {
                fallBackEngines
            }

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
        val installedIdentifiers = installedSearchEngineIdentifiers(context)
        val engineList = searchEngines.await()

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
        val installedIdentifiers = installedSearchEngineIdentifiers(context)
        val engineList = loadedSearchEngines.await()

        engineList.copy(list = engineList.list.filterNot { installedIdentifiers.contains(it.identifier) })
    }

    override suspend fun loadSearchEngines(context: Context): SearchEngineList {
        return installedSearchEngines(context)
    }

    fun installSearchEngine(context: Context, searchEngine: SearchEngine) = runBlocking {
        val installedIdentifiers = installedSearchEngineIdentifiers(context).toMutableSet()
        installedIdentifiers.add(searchEngine.identifier)
        prefs(context).edit().putStringSet(localeAwareInstalledEnginesKey(), installedIdentifiers).apply()
    }

    fun uninstallSearchEngine(context: Context, searchEngine: SearchEngine) = runBlocking {
        val isCustom = CustomSearchEngineStore.isCustomSearchEngine(context, searchEngine.identifier)

        if (isCustom) {
            CustomSearchEngineStore.removeSearchEngine(context, searchEngine.identifier)
        } else {
            val installedIdentifiers = installedSearchEngineIdentifiers(context).toMutableSet()
            installedIdentifiers.remove(searchEngine.identifier)
            prefs(context).edit().putStringSet(localeAwareInstalledEnginesKey(), installedIdentifiers).apply()
        }

        reload()
    }

    fun reload() {
        launch {
            customSearchEngines = async { CustomSearchEngineProvider().loadSearchEngines(context) }
            loadedSearchEngines = refreshAsync()
        }
    }

    // When we change the locale we need to update the baseSearchEngines list
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    open fun updateBaseSearchEngines() {
        baseSearchEngines = async {
            AssetsSearchEngineProvider(localizationProvider).loadSearchEngines(context)
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
        val installedEnginesKey = localeAwareInstalledEnginesKey()

        if (installedEnginesKey != prefs.getString(CURRENT_LOCALE_KEY, "")) {
            updateBaseSearchEngines()
            reload()
            prefs.edit().putString(CURRENT_LOCALE_KEY, installedEnginesKey).apply()
        }

        if (!prefs.contains(installedEnginesKey)) {
            val defaultSet = baseSearchEngines.await()
                .list
                .map { it.identifier }
                .toSet()

            prefs.edit().putStringSet(installedEnginesKey, defaultSet).apply()
        }

        val installedIdentifiers = prefs(context).getStringSet(installedEnginesKey, setOf()) ?: setOf()

        val customEngineIdentifiers = customSearchEngines.await().list.map { it.identifier }.toSet()
        return installedIdentifiers + customEngineIdentifiers
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    suspend fun localeAwareInstalledEnginesKey(): String {
        val tag = localizationProvider.determineRegion().let {
            val region = it.region?.let { region ->
                if (region.isEmpty()) "" else "-$region"
            }

            "${it.languageTag}$region"
        }

        return "$INSTALLED_ENGINES_KEY-$tag"
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    companion object {
        val BUNDLED_SEARCH_ENGINES = listOf("reddit", "youtube")
        const val PREF_FILE = "fenix-search-engine-provider"
        const val INSTALLED_ENGINES_KEY = "fenix-installed-search-engines"
        const val CURRENT_LOCALE_KEY = "fenix-current-locale"
    }
}
