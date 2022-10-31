/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.awesomebar.provider

import android.content.Context
import android.graphics.Bitmap
import mozilla.components.browser.state.search.SearchEngine
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.awesomebar.AwesomeBar
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.fetch.Client
import mozilla.components.concept.fetch.Request
import mozilla.components.concept.fetch.isSuccess
import mozilla.components.feature.awesomebar.facts.emitSearchSuggestionClickedFact
import mozilla.components.feature.search.SearchUseCases
import mozilla.components.feature.search.ext.buildSearchUrl
import mozilla.components.feature.search.suggestions.SearchSuggestionClient
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.ktx.kotlin.sanitizeURL
import java.io.IOException
import java.util.UUID
import java.util.concurrent.TimeUnit

/**
 * A [AwesomeBar.SuggestionProvider] implementation that provides a suggestion containing search engine suggestions (as
 * chips) from the passed in [SearchEngine].
 */
@Suppress("LongParameterList")
class SearchSuggestionProvider private constructor(
    internal val client: SearchSuggestionClient,
    private val searchUseCase: SearchUseCases.SearchUseCase,
    private val limit: Int = 15,
    private val mode: Mode = Mode.SINGLE_SUGGESTION,
    internal val engine: Engine? = null,
    private val icon: Bitmap? = null,
    private val showDescription: Boolean = true,
    private val filterExactMatch: Boolean = false,
) : AwesomeBar.SuggestionProvider {
    override val id: String = UUID.randomUUID().toString()

    init {
        require(limit >= 1) { "limit needs to be >= 1" }
    }

    /**
     * Creates a [SearchSuggestionProvider] for the provided [SearchEngine].
     *
     * @param searchEngine The search engine to request suggestions from.
     * @param searchUseCase The use case to invoke for searches.
     * @param fetchClient The HTTP client for requesting suggestions from the search engine.
     * @param limit The maximum number of suggestions that should be returned. It needs to be >= 1.
     * @param mode Whether to return a single search suggestion (with chips) or one suggestion per item.
     * @param engine optional [Engine] instance to call [Engine.speculativeConnect] for the
     * highest scored search suggestion URL.
     * @param icon The image to display next to the result. If not specified, the engine icon is used.
     * @param showDescription whether or not to add the search engine name as description.
     * @param filterExactMatch If true filters out suggestions that exactly match the entered text.
     * @param private When set to `true` then all requests to search engines will be made in private
     * mode.
     */
    constructor(
        searchEngine: SearchEngine,
        searchUseCase: SearchUseCases.SearchUseCase,
        fetchClient: Client,
        limit: Int = 15,
        mode: Mode = Mode.SINGLE_SUGGESTION,
        engine: Engine? = null,
        icon: Bitmap? = null,
        showDescription: Boolean = true,
        filterExactMatch: Boolean = false,
        private: Boolean = false,
    ) : this (
        SearchSuggestionClient(searchEngine) { url -> fetch(fetchClient, url, private) },
        searchUseCase,
        limit,
        mode,
        engine,
        icon,
        showDescription,
        filterExactMatch,
    )

    /**
     * Creates a [SearchSuggestionProvider] using the default engine as provided by the given
     * [BrowserStore].
     *
     * @param context the activity or application context, required to load search engines.
     * @param store The [BrowserStore] to look up search engines.
     * @param searchUseCase The use case to invoke for searches.
     * @param fetchClient The HTTP client for requesting suggestions from the search engine.
     * @param limit The maximum number of suggestions that should be returned. It needs to be >= 1.
     * @param mode Whether to return a single search suggestion (with chips) or one suggestion per item.
     * @param engine optional [Engine] instance to call [Engine.speculativeConnect] for the
     * highest scored search suggestion URL.
     * @param icon The image to display next to the result. If not specified, the engine icon is used.
     * @param showDescription whether or not to add the search engine name as description.
     * @param filterExactMatch If true filters out suggestions that exactly match the entered text.
     * @param private When set to `true` then all requests to search engines will be made in private
     * mode.
     */
    constructor(
        context: Context,
        store: BrowserStore,
        searchUseCase: SearchUseCases.SearchUseCase,
        fetchClient: Client,
        limit: Int = 15,
        mode: Mode = Mode.SINGLE_SUGGESTION,
        engine: Engine? = null,
        icon: Bitmap? = null,
        showDescription: Boolean = true,
        filterExactMatch: Boolean = false,
        private: Boolean = false,
    ) : this (
        SearchSuggestionClient(context, store) { url -> fetch(fetchClient, url, private) },
        searchUseCase,
        limit,
        mode,
        engine,
        icon,
        showDescription,
        filterExactMatch,
    )

    @Suppress("ReturnCount")
    override suspend fun onInputChanged(text: String): List<AwesomeBar.Suggestion> {
        if (text.isEmpty()) {
            return emptyList()
        }

        val suggestions = fetchSuggestions(text)

        return when (mode) {
            Mode.MULTIPLE_SUGGESTIONS -> createMultipleSuggestions(text, suggestions).also {
                // Call speculativeConnect for URL of first (highest scored) suggestion
                it.firstOrNull()?.title?.let { searchTerms -> maybeCallSpeculativeConnect(searchTerms) }
            }
            Mode.SINGLE_SUGGESTION -> createSingleSearchSuggestion(text, suggestions).also {
                // Call speculativeConnect for URL of first (highest scored) chip
                it.firstOrNull()?.chips?.firstOrNull()?.let { chip -> maybeCallSpeculativeConnect(chip.title) }
            }
        }
    }

    private fun maybeCallSpeculativeConnect(searchTerms: String) {
        client.searchEngine?.let { searchEngine ->
            engine?.speculativeConnect(searchEngine.buildSearchUrl(searchTerms))
        }
    }

    private suspend fun fetchSuggestions(text: String): List<String>? {
        return try {
            client.getSuggestions(text)
        } catch (e: SearchSuggestionClient.FetchException) {
            Logger.info("Could not fetch search suggestions from search engine", e)
            // If we can't fetch search suggestions then just continue with a single suggestion for the entered text
            emptyList()
        } catch (e: SearchSuggestionClient.ResponseParserException) {
            Logger.warn("Could not parse search suggestions from search engine", e)
            // If parsing failed then just continue with a single suggestion for the entered text
            emptyList()
        }
    }

    @Suppress("ComplexMethod")
    private fun createMultipleSuggestions(text: String, result: List<String>?): List<AwesomeBar.Suggestion> {
        val suggestions = mutableListOf<AwesomeBar.Suggestion>()

        val list = (result ?: listOf(text)).toMutableList()
        if (!list.contains(text) && !filterExactMatch) {
            list.add(0, text)
        }

        if (filterExactMatch && list.contains(text)) {
            list.remove(text)
        }

        val description = if (showDescription) {
            client.searchEngine?.name
        } else {
            null
        }

        list.distinct().take(limit).forEachIndexed { index, item ->
            suggestions.add(
                AwesomeBar.Suggestion(
                    provider = this,
                    // We always use the same ID for the entered text so that this suggestion gets replaced "in place".
                    id = if (item == text) ID_OF_ENTERED_TEXT else item,
                    title = item,
                    description = description,
                    // Don't show an autocomplete arrow for the entered text
                    editSuggestion = if (item == text) null else item,
                    icon = icon ?: client.searchEngine?.icon,
                    // Reducing MAX_VALUE by 2: To allow SearchActionProvider to go above and
                    // still have one additional spot above available.
                    score = Int.MAX_VALUE - (index + 2),
                    onSuggestionClicked = {
                        searchUseCase.invoke(item)
                        emitSearchSuggestionClickedFact()
                    },
                ),
            )
        }

        return suggestions
    }

    @Suppress("ComplexCondition")
    private fun createSingleSearchSuggestion(text: String, result: List<String>?): List<AwesomeBar.Suggestion> {
        val chips = mutableListOf<AwesomeBar.Suggestion.Chip>()

        if ((result == null || result.isEmpty() || !result.contains(text)) && !filterExactMatch) {
            // Add the entered text as first suggestion if needed
            chips.add(AwesomeBar.Suggestion.Chip(text))
        }

        result?.take(limit - chips.size)?. forEach { title ->
            if (!filterExactMatch || title != text) {
                chips.add(AwesomeBar.Suggestion.Chip(title))
            }
        }

        return listOf(
            AwesomeBar.Suggestion(
                provider = this,
                id = text,
                title = client.searchEngine?.name,
                chips = chips,
                score = Int.MAX_VALUE,
                icon = icon ?: client.searchEngine?.icon,
                onChipClicked = { chip ->
                    searchUseCase.invoke(chip.title)
                    emitSearchSuggestionClickedFact()
                },
            ),
        )
    }

    enum class Mode {
        SINGLE_SUGGESTION,
        MULTIPLE_SUGGESTIONS,
    }

    companion object {
        private const val READ_TIMEOUT_IN_MS = 2000L
        private const val CONNECT_TIMEOUT_IN_MS = 1000L
        private const val ID_OF_ENTERED_TEXT = "<@@@entered_text_id@@@>"

        @Suppress("ReturnCount", "TooGenericExceptionCaught")
        private fun fetch(fetchClient: Client, url: String, private: Boolean): String? {
            try {
                val request = Request(
                    url = url.sanitizeURL(),
                    readTimeout = Pair(READ_TIMEOUT_IN_MS, TimeUnit.MILLISECONDS),
                    connectTimeout = Pair(CONNECT_TIMEOUT_IN_MS, TimeUnit.MILLISECONDS),
                    private = private,
                )

                val response = fetchClient.fetch(request)
                if (!response.isSuccess) {
                    return null
                }

                return response.use { it.body.string() }
            } catch (e: IOException) {
                return null
            } catch (e: ArrayIndexOutOfBoundsException) {
                // On some devices we are seeing an ArrayIndexOutOfBoundsException being thrown
                // somewhere inside AOSP/okhttp.
                // See: https://github.com/mozilla-mobile/android-components/issues/964
                return null
            }
        }
    }
}
