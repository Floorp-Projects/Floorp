/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.search

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Deferred
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.async
import kotlinx.coroutines.coroutineScope
import kotlinx.coroutines.launch
import kotlinx.coroutines.runBlocking
import mozilla.components.browser.search.provider.AssetsSearchEngineProvider
import mozilla.components.browser.search.provider.SearchEngineList
import mozilla.components.browser.search.provider.SearchEngineProvider
import mozilla.components.browser.search.provider.localization.LocaleSearchLocalizationProvider
import kotlin.coroutines.CoroutineContext

/**
 * This class provides access to a centralized registry of search engines.
 */
@Suppress("TooManyFunctions")
class SearchEngineManager(
    private val providers: List<SearchEngineProvider> = listOf(
            AssetsSearchEngineProvider(LocaleSearchLocalizationProvider())),
    coroutineContext: CoroutineContext = Dispatchers.IO
) {
    private var deferredSearchEngines: Deferred<SearchEngineList>? = null
    private val scope = CoroutineScope(coroutineContext)

    /**
     * This is set by browsers to indicate the users preference of which search engine to use.
     * This overrides the default which may be set by the [SearchEngineProvider] (e.g. via `list.json`)
     */
    var defaultSearchEngine: SearchEngine? = null

    /**
     * Asynchronously load search engines from providers. Inherits caller's [CoroutineContext].
     */
    @Synchronized
    suspend fun loadAsync(context: Context): Deferred<SearchEngineList> = coroutineScope {
        // We might have previous 'load' calls still running; cancel them.
        deferredSearchEngines?.cancel()
        scope.async {
            loadSearchEngines(context)
        }.also { deferredSearchEngines = it }
    }

    /**
     * Asynchronously load search engines from providers. Inherits caller's [CoroutineContext].
     */
    @Synchronized
    @Suppress("DeferredIsResult")
    @Deprecated("Use `loadAsync` instead", ReplaceWith("loadAsync(context)"))
    // TODO remove it from public API
    suspend fun load(context: Context): Deferred<SearchEngineList> = loadAsync(context)

    /**
     * Gets the localized list of search engines and a default search engine from providers.
     *
     * If no previous call was made to [load] or [loadAsync] then calling this method will
     * perform a blocking load.
     */
    private fun getSearchEngineList(context: Context): SearchEngineList = runBlocking {
        getSearchEngineListAsync(context)
    }

    /**
     * Gets the localized list of search engines and a default search engine from providers.
     *
     * If no previous call was made to [load] or [loadAsync] then calling this method will perform
     * a load asynchronously.
     */
    private suspend fun getSearchEngineListAsync(context: Context): SearchEngineList =
        (deferredSearchEngines ?: loadAsync(context))
                .await()

    /**
     * Returns all search engines.
     */
    @Synchronized
    fun getSearchEngines(context: Context): List<SearchEngine> {
        return getSearchEngineList(context).list
    }

    /**
     * Returns all search engines.
     */
    suspend fun getSearchEnginesAsync(context: Context): List<SearchEngine> {
        return getSearchEngineListAsync(context).list
    }

    /**
     * Returns the default search engine.
     *
     * If defaultSearchEngine has not been set, the default engine is set by the search provider,
     * (e.g. as set in `list.json`). If that is not set, then the first search engine listed is
     * returned.
     *
     * Optionally a name can be passed to this method (e.g. from the user's preferences). If
     * a matching search engine was loaded then this search engine will be returned instead.
     */
    @Synchronized
    fun getDefaultSearchEngine(context: Context, name: String = EMPTY): SearchEngine {
        val searchEngineList = getSearchEngineList(context)
        val providedDefault = getProvidedDefaultSearchEngine(context)

        return when (name) {
            EMPTY -> defaultSearchEngine ?: providedDefault
            else -> searchEngineList.list.find { it.name == name } ?: providedDefault
        }
    }

    /**
     * Returns the default search engine.
     *
     * If defaultSearchEngine has not been set, the default engine is set by the search provider,
     * (e.g. as set in `list.json`). If that is not set, then the first search engine listed is
     * returned.
     *
     * Optionally a name can be passed to this method (e.g. from the user's preferences). If
     * a matching search engine was loaded then this search engine will be returned instead.
     */
    suspend fun getDefaultSearchEngineAsync(context: Context, name: String = EMPTY): SearchEngine {
        val searchEngineList = getSearchEngineListAsync(context)
        val providedDefault = getProvidedDefaultSearchEngineAsync(context)

        return when (name) {
            EMPTY -> defaultSearchEngine ?: providedDefault
            else -> searchEngineList.list.find { it.name == name } ?: providedDefault
        }
    }

    /**
     * Returns the provided default search engine or the first search engine if the default
     * is not set.
     */
    @Synchronized
    fun getProvidedDefaultSearchEngine(context: Context): SearchEngine {
        val searchEngineList = getSearchEngineList(context)
        return searchEngineList.default ?: searchEngineList.list[0]
    }

    /**
     * Returns the provided default search engine or the first search engine if the default
     * is not set.
     */
    suspend fun getProvidedDefaultSearchEngineAsync(context: Context): SearchEngine {
        val searchEngineList = getSearchEngineListAsync(context)
        return searchEngineList.default ?: searchEngineList.list[0]
    }

    /**
     * Registers for ACTION_LOCALE_CHANGED broadcasts and automatically reloads the search engines
     * whenever the locale changes.
     */
    fun registerForLocaleUpdates(context: Context) {
        context.registerReceiver(localeChangedReceiver, IntentFilter(Intent.ACTION_LOCALE_CHANGED))
    }

    /**
     * Loads the search engines and defaults from all search engine providers. Some attempt is made
     * to merge these lists. As there can only be one default, the first provider with a default
     * gets to set this [SearchEngineManager] default.
     */
    private suspend fun loadSearchEngines(context: Context): SearchEngineList {
        val deferredSearchEngines = providers.map {
            scope.async {
                it.loadSearchEngines(context)
            }
        }

        val searchEngineLists =
            deferredSearchEngines.map { it.await() }

        val searchEngines = searchEngineLists
            .fold(emptyList<SearchEngine>()) { sum, searchEngineList ->
                sum + searchEngineList.list
            }
            .distinctBy { it.name }

        val defaultSearchEngine = searchEngineLists
            .firstOrNull { it.default != null }?.default

        return SearchEngineList(searchEngines, defaultSearchEngine)
    }

    internal val localeChangedReceiver by lazy {
        object : BroadcastReceiver() {
            override fun onReceive(context: Context, intent: Intent?) {
                scope.launch {
                    loadAsync(context.applicationContext).await()
                }
            }
        }
    }

    companion object {
        private const val EMPTY = ""
    }
}
