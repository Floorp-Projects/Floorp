/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.compose.browser.awesomebar.internal

import android.os.SystemClock
import androidx.compose.runtime.RememberObserver
import androidx.compose.runtime.mutableStateOf
import kotlinx.coroutines.asCoroutineDispatcher
import kotlinx.coroutines.coroutineScope
import kotlinx.coroutines.launch
import mozilla.components.compose.browser.awesomebar.AwesomeBarFacts
import mozilla.components.compose.browser.awesomebar.AwesomeBarFacts.emitAwesomeBarFact
import mozilla.components.concept.awesomebar.AwesomeBar
import mozilla.components.support.base.facts.Action
import mozilla.components.support.base.utils.NamedThreadFactory
import java.util.concurrent.Executors

/**
 * Class responsible for fetching search suggestions and exposing a [state] to observe the current
 * list of suggestions from a composable.
 */
internal class SuggestionFetcher(
    private val providers: List<AwesomeBar.SuggestionProvider>
) : RememberObserver {
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
        // At this point, we have a timing value for a provider.
        // We have a choice here - we can try grouping different timings together for a
        // single user input value, or we can just treat them as entirely independent values.
        // These timings are correlated with each other in a sense that they act on the same
        // inputs, and are executed at the same time. However, our goal here is to track performance
        // of the providers. Each provider acts independently from another; recording their performance
        // at an individual level will allow us to track that performance over time.
        // Tracked value will be reflected both in perceived user experience (how quickly results from
        // a provider show up), and in a purely technical interpretation of how quickly providers
        // fulfill requests.
        // Grouping also poses timing challenges - as user is typing, we're trying to cancel these
        // provider requests. Given that each request can take an arbitrary amount of time to execute,
        // grouping correctly becomes tricky and we run a risk of omitting certain values - or, of
        // adding a bunch of complexity just for the sake of "correct grouping".
        val start = SystemClock.elapsedRealtimeNanos()
        val suggestions = provider.onInputChanged(text)
        val end = SystemClock.elapsedRealtimeNanos()
        emitProviderQueryTimingFact(provider, timingNs = end - start)

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

    override fun onAbandoned() {
        dispatcher.close()
    }

    override fun onForgotten() {
        dispatcher.close()
    }

    override fun onRemembered() = Unit
}

@Suppress("MagicNumber")
internal fun emitProviderQueryTimingFact(provider: AwesomeBar.SuggestionProvider, timingNs: Long) {
    emitAwesomeBarFact(
        Action.INTERACTION,
        AwesomeBarFacts.Items.PROVIDER_DURATION,
        metadata = mapOf(
            // We only care about millisecond precision here, so convert from ns to ms before emitting.
            AwesomeBarFacts.MetadataKeys.DURATION_PAIR to (provider to (timingNs / 1_000_000L))
        )
    )
}
