/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.awesomebar.provider

import android.net.Uri
import androidx.annotation.VisibleForTesting
import androidx.core.net.toUri
import mozilla.components.browser.icons.BrowserIcons
import mozilla.components.browser.icons.IconRequest
import mozilla.components.concept.awesomebar.AwesomeBar
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.storage.HistoryStorage
import mozilla.components.concept.storage.SearchResult
import mozilla.components.feature.awesomebar.facts.emitHistorySuggestionClickedFact
import mozilla.components.feature.session.SessionUseCases
import java.util.UUID

/**
 * Return 20 history suggestions by default.
 */
const val DEFAULT_HISTORY_SUGGESTION_LIMIT = 20

/**
 * Default suggestions limit multiplier when needing to filter results by an external url filter.
 */
@VisibleForTesting
internal const val HISTORY_RESULTS_TO_FILTER_SCALE_FACTOR = 10

/**
 * A [AwesomeBar.SuggestionProvider] implementation that provides suggestions based on the browsing
 * history stored in the [HistoryStorage].
 *
 * @param historyStorage and instance of the [HistoryStorage] used
 * to query matching history records.
 * @param loadUrlUseCase the use case invoked to load the url when the
 * user clicks on the suggestion.
 * @param icons optional instance of [BrowserIcons] to load fav icons
 * for history URLs.
 * @param engine optional [Engine] instance to call [Engine.speculativeConnect] for the
 * highest scored suggestion URL.
 * @param maxNumberOfSuggestions optional parameter to specify the maximum number of returned suggestions,
 * defaults to [DEFAULT_HISTORY_SUGGESTION_LIMIT]
 * @param showEditSuggestion optional parameter to specify if the suggestion should show the edit button
 * @param suggestionsHeader optional parameter to specify if the suggestion should have a header
 * @param resultsUriFilter Optional predicate to filter matching suggestions by URL.
 */
class HistoryStorageSuggestionProvider(
    @get:VisibleForTesting internal val historyStorage: HistoryStorage,
    private val loadUrlUseCase: SessionUseCases.LoadUrlUseCase,
    private val icons: BrowserIcons? = null,
    internal val engine: Engine? = null,
    @get:VisibleForTesting internal var maxNumberOfSuggestions: Int = DEFAULT_HISTORY_SUGGESTION_LIMIT,
    @get:VisibleForTesting val showEditSuggestion: Boolean = true,
    private val suggestionsHeader: String? = null,
    @get:VisibleForTesting val resultsUriFilter: ((Uri) -> Boolean)? = null,
) : AwesomeBar.SuggestionProvider {

    override val id: String = UUID.randomUUID().toString()

    override fun groupTitle(): String? {
        return suggestionsHeader
    }

    override suspend fun onInputChanged(text: String): List<AwesomeBar.Suggestion> {
        historyStorage.cancelReads(text)

        if (text.isEmpty()) {
            return emptyList()
        }

        val suggestions = when (resultsUriFilter) {
            null -> getHistorySuggestions(text)
            else -> getFilteredHistorySuggestions(text, resultsUriFilter)
        }

        suggestions.firstOrNull()?.url?.let { url -> engine?.speculativeConnect(url) }

        return suggestions.into(this, icons, loadUrlUseCase, showEditSuggestion)
    }

    /**
     * Set maximum number of suggestions.
     */
    fun setMaxNumberOfSuggestions(maxNumber: Int) {
        if (maxNumber <= 0) {
            return
        }

        maxNumberOfSuggestions = maxNumber
    }

    /**
     * Get the maximum number of suggestions that will be provided.
     */
    @VisibleForTesting
    fun getMaxNumberOfSuggestions() = maxNumberOfSuggestions

    /**
     * Reset maximum number of suggestions to default.
     */
    fun resetToDefaultMaxSuggestions() {
        maxNumberOfSuggestions = DEFAULT_HISTORY_SUGGESTION_LIMIT
    }

    /**
     * Get up to [maxNumberOfSuggestions] history suggestions matching [query]].
     *
     * @param query String to filter history entry's title or URL by.
     */
    private fun getHistorySuggestions(query: String) = historyStorage
        .getSuggestions(query, maxNumberOfSuggestions)
        // In case of duplicates we want to pick the suggestion with the highest score.
        // See: https://github.com/mozilla/application-services/issues/970
        .sortedByDescending { it.score }
        .distinctBy { it.id }
        .take(maxNumberOfSuggestions)

    /**
     * Get up to [maxNumberOfSuggestions] history suggestions matching [query] and [filter].
     *
     * @param query String to filter history entry's title or URL by.
     * @param filter Predicate to filter the URLs of the history entries that match the [query].
     */
    private fun getFilteredHistorySuggestions(query: String, filter: (Uri) -> Boolean) = historyStorage
        .getSuggestions(query, maxNumberOfSuggestions * HISTORY_RESULTS_TO_FILTER_SCALE_FACTOR)
        .sortedByDescending { it.score }
        .distinctBy { it.id }
        .filter {
            filter(it.url.toUri())
        }
        .take(maxNumberOfSuggestions)
}

internal suspend fun Iterable<SearchResult>.into(
    provider: AwesomeBar.SuggestionProvider,
    icons: BrowserIcons?,
    loadUrlUseCase: SessionUseCases.LoadUrlUseCase,
    showEditSuggestion: Boolean = true,
): List<AwesomeBar.Suggestion> {
    val iconRequests = this.map { icons?.loadIcon(IconRequest(url = it.url, waitOnNetworkLoad = false)) }
    return this.zip(iconRequests) { result, icon ->
        AwesomeBar.Suggestion(
            provider = provider,
            id = result.id,
            icon = icon?.await()?.bitmap,
            title = result.title,
            description = result.url,
            editSuggestion = if (showEditSuggestion) result.url else null,
            score = result.score,
            onSuggestionClicked = {
                loadUrlUseCase.invoke(result.url)
                emitHistorySuggestionClickedFact()
            },
        )
    }
}
