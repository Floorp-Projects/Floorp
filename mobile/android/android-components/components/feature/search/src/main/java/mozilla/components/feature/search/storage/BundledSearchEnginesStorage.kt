/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.search.storage

import android.content.Context
import android.content.res.AssetManager
import kotlinx.coroutines.Deferred
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.async
import kotlinx.coroutines.withContext
import mozilla.components.browser.state.search.RegionState
import mozilla.components.browser.state.search.SearchEngine
import mozilla.components.feature.search.middleware.SearchMiddleware
import mozilla.components.support.ktx.android.content.res.readJSONObject
import mozilla.components.support.ktx.android.org.json.toList
import mozilla.components.support.ktx.android.org.json.tryGetString
import org.json.JSONArray
import org.json.JSONObject
import java.util.Locale

/**
 * A storage implementation for reading bundled [SearchEngine]s from the app's assets.
 */
internal class BundledSearchEnginesStorage(
    private val context: Context
) : SearchMiddleware.BundleStorage {
    /**
     * Load the [SearchMiddleware.BundleStorage.Bundle] for the given [region] and [locale].
     */
    override suspend fun load(
        region: RegionState,
        locale: Locale
    ): SearchMiddleware.BundleStorage.Bundle = withContext(Dispatchers.IO) {
        val localizedConfiguration = loadAndFilterConfiguration(context, region, locale)
        val searchEngineIdentifiers = localizedConfiguration.visibleDefaultEngines

        val searchEngines = loadSearchEnginesFromList(
            context,
            searchEngineIdentifiers.distinct()
        )

        // Reorder the list of search engines according to the configuration.
        // Note: we're using the name of the search engine, not the id, so we can only do this
        // after we've loaded the search engine from the XML
        val searchOrder = localizedConfiguration.searchOrder
        val orderedList = searchOrder
            .map { name ->
                searchEngines.filter { it.name == name }
            }
            .flatten()

        val unorderedRest = searchEngines
            .filter {
                !searchOrder.contains(it.name)
            }

        val defaultEngine = localizedConfiguration.searchDefault?.let { name ->
            searchEngines.find { it.name == name }
        } ?: throw IllegalStateException("No default engine for configuration: locale=$locale, region=$region")

        SearchMiddleware.BundleStorage.Bundle(
            list = orderedList + unorderedRest,
            defaultSearchEngineId = defaultEngine.id
        )
    }
}

private data class SearchEngineListConfiguration(
    val visibleDefaultEngines: List<String>,
    val searchOrder: List<String>,
    val searchDefault: String?
)

private fun loadAndFilterConfiguration(
    context: Context,
    region: RegionState,
    locale: Locale
): SearchEngineListConfiguration {
    val config = context.assets.readJSONObject("search/list.json")

    val configBlocks = pickConfigurationBlocks(locale, config)
    val jsonSearchEngineIdentifiers = getSearchEngineIdentifiersFromBlock(region, locale, configBlocks)

    val searchOrder = getSearchOrderFromBlock(region, configBlocks)
    val searchDefault = getSearchDefaultFromBlock(region, configBlocks)

    return SearchEngineListConfiguration(
        applyOverridesIfNeeded(region, config, jsonSearchEngineIdentifiers),
        searchOrder.toList(),
        searchDefault
    )
}

private fun pickConfigurationBlocks(
    locale: Locale,
    config: JSONObject
): Array<JSONObject> {
    val localesConfig = config.getJSONObject("locales")

    val localizedConfig = when {
        // First try (Locale): locales/xx_XX/
        localesConfig.has(locale.languageTag) ->
            localesConfig.getJSONObject(locale.languageTag)

        // Second try (Language): locales/xx/
        localesConfig.has(locale.language) ->
            localesConfig.getJSONObject(locale.language)

        // Give up, and fallback to defaults
        else -> null
    }

    return localizedConfig?.let {
        arrayOf(it, config)
    } ?: arrayOf(config)
}

private fun getSearchEngineIdentifiersFromBlock(
    region: RegionState,
    locale: Locale,
    configBlocks: Array<JSONObject>
): JSONArray {
    // Now test if there's an override for the region (if it's set)
    return getArrayFromBlock(region, "visibleDefaultEngines", configBlocks)
        ?: throw IllegalStateException("No visibleDefaultEngines using region $region and locale $locale")
}

private fun getSearchDefaultFromBlock(
    region: RegionState,
    configBlocks: Array<JSONObject>
): String? = getValueFromBlock(region, configBlocks) {
    it.tryGetString("searchDefault")
}

private fun getSearchOrderFromBlock(
    region: RegionState,
    configBlocks: Array<JSONObject>
): JSONArray? = getArrayFromBlock(region, "searchOrder", configBlocks)

private fun getArrayFromBlock(
    region: RegionState,
    key: String,
    blocks: Array<JSONObject>
): JSONArray? = getValueFromBlock(region, blocks) {
    it.optJSONArray(key)
}

/**
 * This looks for a JSONObject in the config blocks it is passed that is able to be transformed
 * into a value. It tries the permutations of locale and region from most specific to least
 * specific.
 *
 * This has to be done on a value basis, not a configBlock basis, as the configuration for a
 * given locale/region is not grouped into one object, but spread across the json file,
 * according to these rules.
 */
private fun <T : Any> getValueFromBlock(
    region: RegionState,
    blocks: Array<JSONObject>,
    transform: (JSONObject) -> T?
): T? {
    val regions = arrayOf(region.home, "default")

    return blocks
        .flatMap { block ->
            regions.mapNotNull { region -> block.optJSONObject(region) }
        }
        .mapNotNull(transform)
        .firstOrNull()
}

private fun applyOverridesIfNeeded(
    region: RegionState,
    config: JSONObject,
    jsonSearchEngineIdentifiers: JSONArray
): List<String> {
    val overrides = config.getJSONObject("regionOverrides")
    val searchEngineIdentifiers = mutableListOf<String>()
    val regionOverrides = if (overrides.has(region.home)) {
        overrides.getJSONObject(region.home)
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

private suspend fun loadSearchEnginesFromList(
    context: Context,
    searchEngineIdentifiers: List<String>
): List<SearchEngine> {
    val assets = context.assets
    val reader = SearchEngineReader()

    val deferredSearchEngines = mutableListOf<Deferred<SearchEngine>>()

    searchEngineIdentifiers.forEach { identifier ->
        deferredSearchEngines.add(GlobalScope.async(Dispatchers.IO) {
            loadSearchEngine(assets, reader, identifier)
        })
    }

    return deferredSearchEngines.map { it.await() }
}

private fun loadSearchEngine(
    assets: AssetManager,
    reader: SearchEngineReader,
    identifier: String
): SearchEngine = assets.open("searchplugins/$identifier.xml").use { stream ->
    reader.loadStream(identifier, stream)
}

private val Locale.languageTag: String
    get() = "$language-$country"
