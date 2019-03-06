/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.awesomebar.provider

import mozilla.components.browser.search.SearchEngine
import mozilla.components.browser.search.suggestions.SearchSuggestionClient
import mozilla.components.concept.awesomebar.AwesomeBar
import mozilla.components.concept.fetch.Client
import mozilla.components.concept.fetch.Request
import mozilla.components.concept.fetch.isSuccess
import mozilla.components.feature.search.SearchUseCases
import mozilla.components.support.base.log.logger.Logger
import java.io.IOException
import java.util.UUID
import java.util.concurrent.TimeUnit

/**
 * A [AwesomeBar.SuggestionProvider] implementation that provides a suggestion containing search engine suggestions (as
 * chips) from the passed in [SearchEngine].
 */
class SearchSuggestionProvider(
    private val searchEngine: SearchEngine,
    private val searchUseCase: SearchUseCases.SearchUseCase,
    private val fetchClient: Client,
    private val mode: Mode = Mode.SINGLE_SUGGESTION
) : AwesomeBar.SuggestionProvider {
    override val id: String = UUID.randomUUID().toString()

    private val client = if (searchEngine.canProvideSearchSuggestions) {
        SearchSuggestionClient(searchEngine) {
            url -> fetch(url)
        }
    } else {
        null
    }

    @Suppress("ReturnCount")
    override suspend fun onInputChanged(text: String): List<AwesomeBar.Suggestion> {
        if (text.isEmpty()) {
            return emptyList()
        }

        val suggestions = fetchSuggestions(text)

        return when (mode) {
            Mode.MULTIPLE_SUGGESTIONS -> createMultipleSuggestions(text, suggestions)
            Mode.SINGLE_SUGGESTION -> createSingleSearchSuggestion(text, suggestions)
        }
    }

    private suspend fun fetchSuggestions(text: String): List<String>? {
        if (client == null) {
            // This search engine doesn't support suggestions. Let's only return a default suggestion
            // for the entered text.
            return emptyList()
        }

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

    private fun createMultipleSuggestions(text: String, result: List<String>?): List<AwesomeBar.Suggestion> {
        val suggestions = mutableListOf<AwesomeBar.Suggestion>()

        val list = (result ?: listOf(text)).toMutableList()
        if (!list.contains(text)) {
            list.add(0, text)
        }

        list.forEachIndexed { index, item ->
            suggestions.add(AwesomeBar.Suggestion(
                provider = this,
                // We always use the same ID for the entered text so that this suggestion gets replaced "in place".
                id = if (item == text) ID_OF_ENTERED_TEXT else item,
                title = item,
                description = searchEngine.name,
                icon = { _, _ ->
                    searchEngine.icon
                },
                score = Int.MAX_VALUE - index,
                onSuggestionClicked = {
                    searchUseCase.invoke(item)
                }
            ))
        }

        return suggestions
    }

    private fun createSingleSearchSuggestion(text: String, result: List<String>?): List<AwesomeBar.Suggestion> {
        val chips = mutableListOf<AwesomeBar.Suggestion.Chip>()

        if (result == null || result.isEmpty() || !result.contains(text)) {
            // Add the entered text as first suggestion if needed
            chips.add(AwesomeBar.Suggestion.Chip(text))
        }

        result?.forEach { title ->
            chips.add(AwesomeBar.Suggestion.Chip(title))
        }

        return listOf(AwesomeBar.Suggestion(
            provider = this,
            id = text,
            title = searchEngine.name,
            chips = chips,
            score = Int.MAX_VALUE,
            icon = { _, _ ->
                searchEngine.icon
            },
            onChipClicked = { chip ->
                searchUseCase.invoke(chip.title)
            }
        ))
    }

    override val shouldClearSuggestions: Boolean
        // We do not want the suggestion of this provider to disappear and re-appear when text changes.
        get() = false

    enum class Mode {
        SINGLE_SUGGESTION,
        MULTIPLE_SUGGESTIONS
    }

    @Suppress("ReturnCount", "TooGenericExceptionCaught")
    private fun fetch(url: String): String? {
        try {
            val request = Request(
                url = url,
                readTimeout = Pair(READ_TIMEOUT_IN_MS, TimeUnit.MILLISECONDS),
                connectTimeout = Pair(CONNECT_TIMEOUT_IN_MS, TimeUnit.MILLISECONDS)
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

    companion object {
        private const val READ_TIMEOUT_IN_MS = 2000L
        private const val CONNECT_TIMEOUT_IN_MS = 1000L
        private const val ID_OF_ENTERED_TEXT = "<@@@entered_text_id@@@>"
    }
}
