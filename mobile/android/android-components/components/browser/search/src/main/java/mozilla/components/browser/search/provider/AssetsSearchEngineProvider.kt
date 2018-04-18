/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.search.provider

import android.content.Context
import android.content.res.AssetManager
import kotlinx.coroutines.experimental.Deferred
import kotlinx.coroutines.experimental.async
import mozilla.components.browser.search.SearchEngine
import mozilla.components.browser.search.SearchEngineParser
import mozilla.components.browser.search.provider.filter.SearchEngineFilter
import mozilla.components.browser.search.provider.localization.SearchLocalizationProvider
import mozilla.components.support.ktx.android.content.res.readJSONObject
import org.json.JSONArray
import org.json.JSONObject

/**
 * SearchEngineProvider implementation to load the included search engines from assets.
 */
class AssetsSearchEngineProvider(
    private val localizationProvider: SearchLocalizationProvider,
    private val filters: List<SearchEngineFilter> = emptyList()
) : SearchEngineProvider {

    /**
     * Load search engines from this provider.
     */
    override suspend fun loadSearchEngines(context: Context): List<SearchEngine> {
        val searchEngineIdentifiers = loadAndFilterConfiguration(context)
        return loadSearchEnginesFromList(context, searchEngineIdentifiers)
    }

    private suspend fun loadSearchEnginesFromList(
        context: Context,
        searchEngineIdentifiers: List<String>
    ): List<SearchEngine> {
        val searchEngines = mutableListOf<SearchEngine>()

        val assets = context.assets
        val parser = SearchEngineParser()

        val deferredSearchEngines = mutableListOf<Deferred<SearchEngine>>()

        searchEngineIdentifiers.forEach {
            deferredSearchEngines.add(async {
                loadSearchEngine(assets, parser, it)
            })
        }

        deferredSearchEngines.forEach {
            val searchEngine = it.await()
            if (shouldBeFiltered(searchEngine)) {
                searchEngines.add(searchEngine)
            }
        }

        return searchEngines
    }

    private fun shouldBeFiltered(searchEngine: SearchEngine): Boolean {
        filters.forEach {
            if (!it.filter(searchEngine)) {
                return false
            }
        }

        return true
    }

    private fun loadSearchEngine(
        assets: AssetManager,
        parser: SearchEngineParser,
        identifier: String
    ): SearchEngine {
        val fileName = "$identifier.xml"

        return parser.load(assets, identifier, "searchplugins/$fileName")
    }

    private fun loadAndFilterConfiguration(context: Context): List<String> {
        val config = context.assets.readJSONObject("search/list.json")

        val configBlock = pickConfigurationBlock(config)
        val jsonSearchEngineIdentifiers = getSearchEngineIdentifiersFromBlock(configBlock)

        return applyOverridesIfNeeded(config, jsonSearchEngineIdentifiers)
    }

    private fun pickConfigurationBlock(config: JSONObject): JSONObject {
        val localesConfig = config.getJSONObject("locales")

        return when {
            // First try (Locale): locales/xx_XX/
            localesConfig.has(localizationProvider.languageTag) ->
                localesConfig.getJSONObject(localizationProvider.languageTag)

            // Second try (Language): locales/xx/
            localesConfig.has(localizationProvider.language) ->
                localesConfig.getJSONObject(localizationProvider.language)

            // Third try: Use default
            else -> config
        }
    }

    private fun getSearchEngineIdentifiersFromBlock(configBlock: JSONObject): JSONArray {
        // Now test if there's an override for the region (if it's set)
        return if (localizationProvider.region != null && configBlock.has(localizationProvider.region)) {
            configBlock.getJSONObject(localizationProvider.region)
        } else {
            configBlock.getJSONObject("default")
        }.getJSONArray("visibleDefaultEngines")
    }

    private fun applyOverridesIfNeeded(config: JSONObject, jsonSearchEngineIdentifiers: JSONArray): List<String> {
        val overrides = config.getJSONObject("regionOverrides")
        val searchEngineIdentifiers = mutableListOf<String>()
        val regionOverrides = if (localizationProvider.region != null && overrides.has(localizationProvider.region)) {
            overrides.getJSONObject(localizationProvider.region)
        } else {
            null
        }

        for (i in 0 until jsonSearchEngineIdentifiers.length()) {
            var identifier = jsonSearchEngineIdentifiers.getString(i)
            if (regionOverrides != null && regionOverrides.has(identifier)) {
                identifier = regionOverrides.getString(identifier)
            }
            searchEngineIdentifiers.add(identifier)
        }

        return searchEngineIdentifiers
    }
}
