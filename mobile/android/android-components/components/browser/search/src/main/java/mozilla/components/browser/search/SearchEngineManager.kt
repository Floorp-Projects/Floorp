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
import mozilla.components.browser.search.provider.SearchEngineProvider
import mozilla.components.browser.search.provider.localization.LocaleSearchLocalizationProvider

/**
 * This class provides access to a centralized registry of search engines.
 */
class SearchEngineManager(
    private val providers: List<SearchEngineProvider> = listOf(
            AssetsSearchEngineProvider(LocaleSearchLocalizationProvider()))
) {
    private var deferredSearchEngines: Deferred<List<SearchEngine>>? = null
    private val scope = CoroutineScope(Dispatchers.Default)
    var defaultSearchEngine: SearchEngine? = null

    /**
     * Asynchronously load search engines from providers. Inherits caller's [CoroutineScope.coroutineContext].
     */
    @Synchronized
    suspend fun load(context: Context): Deferred<List<SearchEngine>> = coroutineScope {
        // We might have previous 'load' calls still running; cancel them.
        deferredSearchEngines?.cancel()
        scope.async {
            loadSearchEngines(context)
        }.also { deferredSearchEngines = it }
    }

    /**
     * Returns all search engines. If no call to load() has been made then calling this method
     * will perform a load.
     */
    @Synchronized
    fun getSearchEngines(context: Context): List<SearchEngine> {
        deferredSearchEngines?.let { return runBlocking { it.await() } }

        return runBlocking { load(context).await() }
    }

    /**
     * Returns the default search engine.
     *
     * If defaultSearchEngine has not been set, the default engine is the first engine loaded by the
     * first provider passed to the constructor of SearchEngineManager.
     *
     * Optionally a name can be passed to this method (e.g. from the user's preferences). If
     * a matching search engine was loaded then this search engine will be returned instead.
     */
    @Synchronized
    fun getDefaultSearchEngine(context: Context, name: String = EMPTY): SearchEngine {
        val searchEngines = getSearchEngines(context)

        return when (name) {
            EMPTY -> defaultSearchEngine ?: searchEngines[0]
            else -> searchEngines.find { it.name == name } ?: searchEngines[0]
        }
    }

    /**
     * Returns search engine corresponding to name passed to this method, or null if not found.
     */
    @Synchronized
    fun getSearchEngineByName(context: Context, name: String): SearchEngine? {
        val searchEngines = getSearchEngines(context)

        return searchEngines.find { it.name == name }
    }

    /**
     * Registers for ACTION_LOCALE_CHANGED broadcasts and automatically reloads the search engines
     * whenever the locale changes.
     */
    fun registerForLocaleUpdates(context: Context) {
        context.registerReceiver(localeChangedReceiver, IntentFilter(Intent.ACTION_LOCALE_CHANGED))
    }

    private suspend fun loadSearchEngines(context: Context): List<SearchEngine> {
        val searchEngines = mutableListOf<SearchEngine>()
        val deferredSearchEngines = mutableListOf<Deferred<List<SearchEngine>>>()

        providers.forEach {
            deferredSearchEngines.add(scope.async {
                it.loadSearchEngines(context)
            })
        }

        deferredSearchEngines.forEach {
            searchEngines.addAll(it.await())
        }

        return searchEngines
    }

    private val localeChangedReceiver by lazy {
        object : BroadcastReceiver() {
            override fun onReceive(context: Context, intent: Intent?) {
                scope.launch {
                    load(context.applicationContext).await()
                }
            }
        }
    }

    companion object {
        private const val EMPTY = ""
    }
}
