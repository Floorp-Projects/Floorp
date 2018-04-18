/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.search

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import kotlinx.coroutines.experimental.CommonPool
import kotlinx.coroutines.experimental.Deferred
import kotlinx.coroutines.experimental.async
import kotlinx.coroutines.experimental.launch
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

    /**
     * Asynchronously load search engines from providers.
     */
    @Synchronized
    suspend fun load(context: Context): Deferred<List<SearchEngine>> {
        return async {
            loadSearchEngines(context)
        }.also { deferredSearchEngines = it }
    }

    /**
     * Returns all search engines. If no call to load() has been made then calling this method
     * will perform a load.
     */
    @Synchronized
    suspend fun getSearchEngines(context: Context): List<SearchEngine> {
        deferredSearchEngines?.let { return it.await() }

        return load(context).await()
    }

    /**
     * Returns the default search engine.
     *
     * The default engine is the first engine loaded by the first provider passed to the
     * constructor of SearchEngineManager.
     *
     * Optionally an identifier can be passed to this method (e.g. from the user's preferences). If
     * a matching search engine was loaded then this search engine will be returned instead.
     */
    @Synchronized
    suspend fun getDefaultSearchEngine(context: Context, identifier: String = EMPTY): SearchEngine {
        val searchEngines = getSearchEngines(context)

        return when (identifier) {
            EMPTY -> searchEngines[0]
            else -> searchEngines.find { it.identifier == identifier } ?: searchEngines[0]
        }
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
            deferredSearchEngines.add(async {
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
                launch(CommonPool) {
                    load(context.applicationContext)
                }
            }
        }
    }

    companion object {
        private const val EMPTY = ""
    }
}
