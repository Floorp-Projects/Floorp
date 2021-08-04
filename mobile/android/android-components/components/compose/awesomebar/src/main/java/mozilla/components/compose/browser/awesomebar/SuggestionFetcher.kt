/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.compose.browser.awesomebar

import androidx.compose.runtime.mutableStateOf
import kotlinx.coroutines.asCoroutineDispatcher
import kotlinx.coroutines.coroutineScope
import kotlinx.coroutines.launch
import mozilla.components.concept.awesomebar.AwesomeBar
import mozilla.components.support.base.utils.NamedThreadFactory
import java.util.concurrent.Executors

/**
 * Class responsible for fetching search suggestions and exposing a [state] to observe the current
 * list of suggestions from a composable.
 */
internal class SuggestionFetcher(
    private val providers: List<AwesomeBar.SuggestionProvider>
) {
    private val dispatcher = Executors.newFixedThreadPool(
        providers.size,
        NamedThreadFactory("SuggestionFetcher")
    ).asCoroutineDispatcher()

    /**
     * The current list of suggestions as an observable list.
     */
    val state = mutableStateOf<List<AwesomeBar.Suggestion>>(emptyList())

    /**
     * Fetches suggestions for [text] from all [providers] asynchronously.
     *
     * The [state] property will be updated whenever new suggestions are available.
     */
    suspend fun fetch(text: String) {
        coroutineScope {
            providers.forEach { provider ->
                launch(dispatcher) { fetchFrom(provider, text) }
            }
        }
    }

    /**
     * Fetches suggestions from [provider].
     */
    private suspend fun fetchFrom(
        provider: AwesomeBar.SuggestionProvider,
        text: String
    ) {
        val suggestions = provider.onInputChanged(text)
        processResultFrom(provider, suggestions)
    }

    /**
     * Updates [state] to include the [suggestions] from [provider].
     */
    @Synchronized
    private fun processResultFrom(
        provider: AwesomeBar.SuggestionProvider,
        suggestions: List<AwesomeBar.Suggestion>
    ) {
        val updatedSuggestions = state.value
            .filter { suggestion -> suggestion.provider != provider }
            .toMutableList()

        updatedSuggestions.addAll(suggestions)
        updatedSuggestions.sortByDescending { suggestion -> suggestion.score }

        state.value = updatedSuggestions
    }
}
