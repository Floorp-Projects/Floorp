/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.search.provider

import android.content.Context
import android.content.res.AssetManager
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Deferred
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.async
import mozilla.components.browser.search.SearchEngine
import mozilla.components.browser.search.SearchEngineParser
import mozilla.components.browser.search.provider.filter.SearchEngineFilter
import mozilla.components.browser.search.provider.localization.SearchLocalization
import mozilla.components.browser.search.provider.localization.SearchLocalizationProvider
import mozilla.components.support.ktx.android.content.res.readJSONObject
import mozilla.components.support.ktx.android.org.json.toList
import mozilla.components.support.ktx.android.org.json.tryGetString
import org.json.JSONArray
import org.json.JSONObject

/**
 * SearchEngineProvider implementation to load the included search engines from assets.
 *
 * A SearchLocalizationProvider implementation is used to customize the returned search engines for
 * the language and country of the user/device.
 *
 * Optionally SearchEngineFilter instances can be provided to remove unwanted search engines from
 * the loaded list.
 *
 * Optionally <code>additionalIdentifiers</code> to be loaded can be specified. A search engine
 * identifier corresponds to the search plugin XML file name (e.g. duckduckgo -> duckduckgo.xml).
 */
@Suppress("TooManyFunctions", "LargeClass")
class AssetsSearchEngineProvider(
    private val localizationProvider: SearchLocalizationProvider,
    private val filters: List<SearchEngineFilter> = emptyList(),
    private val additionalIdentifiers: List<String> = emptyList()
) : SearchEngineProvider {
    private val scope = CoroutineScope(Dispatchers.IO)

    /**
     * Load search engines from this provider.
     */
    override suspend fun loadSearchEngines(context: Context): SearchEngineList {
        val localization = localizationProvider.determineRegion()
        val localizedConfiguration = loadAndFilterConfiguration(context, localization)
        val searchEngineIdentifiers = localizedConfiguration.visibleDefaultEngines + additionalIdentifiers

        val searchEngines = loadSearchEnginesFromList(
            context,
            searchEngineIdentifiers.distinct()
        )

        // Reorder the list of search engines according to the configuration.
        // Note: we're using the name of the search engine, not the id, so we can only do this
        // after we've loaded the search engine from the XML
        val searchOrder = localizedConfiguration.searchOrder
        val orderedList = searchOrder
            .mapNotNull { name ->
                searchEngines.filter { it.name == name }
            }
            .flatten()

        val unorderedRest = searchEngines
            .filter {
                !searchOrder.contains(it.name)
            }

        val defaultEngine = localizedConfiguration.searchDefault?.let { name ->
                searchEngines.find { it.name == name }
            }

        return SearchEngineList(
            orderedList + unorderedRest,
            defaultEngine
        )
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
            deferredSearchEngines.add(scope.async {
                loadSearchEngine(assets, parser, it)
            })
        }

        deferredSearchEngines.forEach {
            val searchEngine = it.await()
            if (shouldBeFiltered(context, searchEngine)) {
                searchEngines.add(searchEngine)
            }
        }

        return searchEngines
    }

    private fun shouldBeFiltered(context: Context, searchEngine: SearchEngine): Boolean {
        filters.forEach {
            if (!it.filter(context, searchEngine)) {
                return false
            }
        }

        return true
    }

    private fun loadSearchEngine(
        assets: AssetManager,
        parser: SearchEngineParser,
        identifier: String
    ): SearchEngine = parser.load(assets, identifier, "searchplugins/$identifier.xml")

    private fun loadAndFilterConfiguration(
        context: Context,
        localization: SearchLocalization
    ): SearchEngineListConfiguration {
        val config = context.assets.readJSONObject("search/list.json")

        val configBlocks = pickConfigurationBlocks(localization, config)
        val jsonSearchEngineIdentifiers = getSearchEngineIdentifiersFromBlock(localization, configBlocks)

        val searchOrder = getSearchOrderFromBlock(localization, configBlocks)
        val searchDefault = getSearchDefaultFromBlock(localization, configBlocks)

        return SearchEngineListConfiguration(
            applyOverridesIfNeeded(localization, config, jsonSearchEngineIdentifiers),
            searchOrder.toList(),
            searchDefault
        )
    }

    private fun pickConfigurationBlocks(
        localization: SearchLocalization,
        config: JSONObject
    ): Array<JSONObject> {
        val localesConfig = config.getJSONObject("locales")

        val localizedConfig = when {
            // First try (Locale): locales/xx_XX/
            localesConfig.has(localization.languageTag) ->
                localesConfig.getJSONObject(localization.languageTag)

            // Second try (Language): locales/xx/
            localesConfig.has(localization.language) ->
                localesConfig.getJSONObject(localization.language)

            // Give up, and fallback to defaults
            else -> null
        }

        return localizedConfig?.let {
            arrayOf(it, config)
        } ?: arrayOf(config)
    }

    private fun getSearchEngineIdentifiersFromBlock(
        localization: SearchLocalization,
        configBlocks: Array<JSONObject>
    ): JSONArray {
        // Now test if there's an override for the region (if it's set)
        return getArrayFromBlock(localization, "visibleDefaultEngines", configBlocks)
            ?: throw IllegalStateException("No visibleDefaultEngines for " +
                    "${localization.languageTag} in " +
                    (localization.region ?: "default"))
    }

    private fun getSearchDefaultFromBlock(
        localization: SearchLocalization,
        configBlocks: Array<JSONObject>
    ): String? = getStringFromBlock(localization, "searchDefault", configBlocks)

    private fun getSearchOrderFromBlock(
        localization: SearchLocalization,
        configBlocks: Array<JSONObject>
    ): JSONArray? = getArrayFromBlock(localization, "searchOrder", configBlocks)

    private fun getStringFromBlock(
        localization: SearchLocalization,
        key: String,
        blocks: Array<JSONObject>
    ): String? = getValueFromBlock(localization, blocks) {
        it.tryGetString(key)
    }

    private fun getArrayFromBlock(
        localization: SearchLocalization,
        key: String,
        blocks: Array<JSONObject>
    ): JSONArray? = getValueFromBlock(localization, blocks) {
        it.optJSONArray(key)
    }

    /**
     * This looks for a JSONObject in the config blocks it is passed that is able to be transformed
     * into a value. It tries the permutations of locale and region specific from most specific to
     * least specific.
     *
     * This has to be done on a value basis, not a configBlock basis, as the configuration for a
     * given locale/region is not grouped into one object, but spread across the json file,
     * according to these rules.
     */
    private fun <T : Any> getValueFromBlock(
        localization: SearchLocalization,
        blocks: Array<JSONObject>,
        transform: (JSONObject) -> T?
    ): T? {
        val regions = arrayOf(localization.region, "default")
            .mapNotNull { it }

        return blocks
            .flatMap { block ->
                regions.mapNotNull { region -> block.optJSONObject(region) }
            }
            .mapNotNull(transform)
            .firstOrNull()
    }

    private fun applyOverridesIfNeeded(
        localization: SearchLocalization,
        config: JSONObject,
        jsonSearchEngineIdentifiers: JSONArray
    ): List<String> {
        val overrides = config.getJSONObject("regionOverrides")
        val searchEngineIdentifiers = mutableListOf<String>()
        val region = localization.region
        val regionOverrides = if (region != null && overrides.has(region)) {
            overrides.getJSONObject(region)
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

internal data class SearchEngineListConfiguration(
    val visibleDefaultEngines: List<String>,
    val searchOrder: List<String>,
    val searchDefault: String?
)
