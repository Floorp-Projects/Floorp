/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.search.storage

import android.content.Context
import android.content.res.AssetManager
import kotlinx.coroutines.Deferred
import kotlinx.coroutines.DelicateCoroutinesApi
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.async
import kotlinx.coroutines.withContext
import mozilla.components.browser.state.search.RegionState
import mozilla.components.browser.state.search.SearchEngine
import mozilla.components.feature.search.middleware.SearchMiddleware
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.ktx.android.content.res.readJSONObject
import mozilla.components.support.ktx.android.org.json.toList
import mozilla.components.support.ktx.android.org.json.tryGetString
import org.json.JSONArray
import org.json.JSONObject
import java.util.Locale
import kotlin.coroutines.CoroutineContext

private val logger = Logger("BundledSearchEnginesStorage")

/**
 * A storage implementation for reading bundled [SearchEngine]s from the app's assets.
 */
internal class BundledSearchEnginesStorage(
    private val context: Context,
) : SearchMiddleware.BundleStorage {
    /**
     * Load the [SearchMiddleware.BundleStorage.Bundle] for the given [region] and [locale].
     */
    override suspend fun load(
        region: RegionState,
        locale: Locale,
        coroutineContext: CoroutineContext,
    ): SearchMiddleware.BundleStorage.Bundle = withContext(coroutineContext) {
        val localizedConfiguration = loadAndFilterConfiguration(context, region, locale)
        val searchEngineIdentifiers = localizedConfiguration.visibleDefaultEngines

        val searchEngines = loadSearchEnginesFromList(
            context,
            searchEngineIdentifiers.distinct(),
            SearchEngine.Type.BUNDLED,
            coroutineContext,
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
            defaultSearchEngineId = defaultEngine.id,
        )
    }

    override suspend fun load(
        ids: List<String>,
        coroutineContext: CoroutineContext,
    ): List<SearchEngine> = withContext(coroutineContext) {
        if (ids.isEmpty()) {
            emptyList()
        } else {
            loadSearchEnginesFromList(
                context,
                ids.distinct(),
                SearchEngine.Type.BUNDLED_ADDITIONAL,
                coroutineContext,
            )
        }
    }
}

private data class SearchEngineListConfiguration(
    val visibleDefaultEngines: List<String>,
    val searchOrder: List<String>,
    val searchDefault: String?,
)

private fun loadAndFilterConfiguration(
    context: Context,
    region: RegionState,
    locale: Locale,
): SearchEngineListConfiguration {
    val config = context.assets.readJSONObject("search/list.json")

    val configBlocks = pickConfigurationBlocks(locale, config)
    val jsonSearchEngineIdentifiers = getSearchEngineIdentifiersFromBlock(region, locale, configBlocks)

    val searchOrder = getSearchOrderFromBlock(region, configBlocks)
    val searchDefault = getSearchDefaultFromBlock(region, configBlocks)

    return SearchEngineListConfiguration(
        applyOverridesIfNeeded(region, config, jsonSearchEngineIdentifiers),
        searchOrder.toList(),
        searchDefault,
    )
}

private fun pickConfigurationBlocks(
    locale: Locale,
    config: JSONObject,
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
    configBlocks: Array<JSONObject>,
): JSONArray {
    // Now test if there's an override for the region (if it's set)
    return getArrayFromBlock(region, "visibleDefaultEngines", configBlocks)
        ?: throw IllegalStateException("No visibleDefaultEngines using region $region and locale $locale")
}

private fun getSearchDefaultFromBlock(
    region: RegionState,
    configBlocks: Array<JSONObject>,
): String? = getValueFromBlock(region, configBlocks) {
    it.tryGetString("searchDefault")
}

private fun getSearchOrderFromBlock(
    region: RegionState,
    configBlocks: Array<JSONObject>,
): JSONArray? = getArrayFromBlock(region, "searchOrder", configBlocks)

private fun getArrayFromBlock(
    region: RegionState,
    key: String,
    blocks: Array<JSONObject>,
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
    transform: (JSONObject) -> T?,
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
    jsonSearchEngineIdentifiers: JSONArray,
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

@OptIn(DelicateCoroutinesApi::class)
private suspend fun loadSearchEnginesFromList(
    context: Context,
    searchEngineIdentifiers: List<String>,
    type: SearchEngine.Type,
    coroutineContext: CoroutineContext,
): List<SearchEngine> {
    val assets = context.assets
    val reader = SearchEngineReader(type)

    val deferredSearchEngines = mutableListOf<Deferred<SearchEngine?>>()

    searchEngineIdentifiers.forEach { identifier ->
        deferredSearchEngines.add(
            GlobalScope.async(coroutineContext) {
                loadSearchEngine(assets, reader, identifier)
            },
        )
    }

    return deferredSearchEngines.mapNotNull { it.await() }
}

@Suppress("TooGenericExceptionCaught")
private fun loadSearchEngine(
    assets: AssetManager,
    reader: SearchEngineReader,
    identifier: String,
): SearchEngine? = try {
    assets.open("searchplugins/$identifier.xml").use { stream ->
        reader.loadStream(identifier, stream)
    }
} catch (e: Exception) {
    // Handling all exceptions here (instead of just IOExceptions) as we're
    // seeing crashes we can't explain currently. Letting the app launch
    // will eventually help us understand the root cause:
    // https://github.com/mozilla-mobile/android-components/issues/12304

    // We should also consider logging these errors to Sentry:
    // https://github.com/mozilla-mobile/android-components/issues/12313
    logger.error("Could not load additional search engine with ID $identifier", e)
    null
}

private val Locale.languageTag: String
    get() = "$language-$country"
