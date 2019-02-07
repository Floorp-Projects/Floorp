/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.awesomebar.provider

import mozilla.components.browser.search.SearchEngine
import mozilla.components.browser.search.suggestions.SearchSuggestionClient
import mozilla.components.concept.awesomebar.AwesomeBar
import mozilla.components.feature.search.SearchUseCases
import mozilla.components.support.base.log.logger.Logger
import java.io.IOException
import java.net.HttpURLConnection
import java.net.URL

/**
 * A [AwesomeBar.SuggestionProvider] implementation that provides a suggestion containing search engine suggestions (as
 * chips) from the passed in [SearchEngine].
 */
class SearchSuggestionProvider(
    private val searchEngine: SearchEngine,
    private val searchUseCase: SearchUseCases.SearchUseCase,
    private val mode: Mode = Mode.SINGLE_SUGGESTION
) : AwesomeBar.SuggestionProvider {
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

        return if (mode == Mode.SINGLE_SUGGESTION) {
            createSingleSearchSuggestion(text, suggestions)
        } else {
            createMultipleSuggestions(text, suggestions)
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
            id = "mozac-browser-search-" + searchEngine.identifier,
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
}

private const val READ_TIMEOUT = 2000
private const val CONNECT_TIMEOUT = 1000

// To be replaced with http/fetch dependency:
// https://github.com/mozilla-mobile/android-components/issues/1012
@Suppress("ReturnCount", "TooGenericExceptionCaught", "MagicNumber")
private fun fetch(url: String): String? {
    var urlConnection: HttpURLConnection? = null
    try {
        urlConnection = URL(url).openConnection() as HttpURLConnection
        urlConnection.requestMethod = "GET"
        urlConnection.useCaches = false
        urlConnection.readTimeout = READ_TIMEOUT
        urlConnection.connectTimeout = CONNECT_TIMEOUT

        val responseCode = urlConnection.responseCode
        if (responseCode !in 200..299) {
            return null
        }

        return urlConnection.inputStream.bufferedReader().use { it.readText() }
    } catch (e: IOException) {
        return null
    } catch (e: ClassCastException) {
        return null
    } catch (e: ArrayIndexOutOfBoundsException) {
        // On some devices we are seeing an ArrayIndexOutOfBoundsException being thrown
        // somewhere inside AOSP/okhttp.
        // See: https://github.com/mozilla-mobile/android-components/issues/964
        return null
    } finally {
        urlConnection?.disconnect()
    }
}
