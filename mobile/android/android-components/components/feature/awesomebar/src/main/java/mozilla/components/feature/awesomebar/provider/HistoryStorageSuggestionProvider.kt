/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.awesomebar.provider

import mozilla.components.browser.icons.BrowserIcons
import mozilla.components.browser.icons.IconRequest
import mozilla.components.concept.awesomebar.AwesomeBar
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.storage.HistoryStorage
import mozilla.components.concept.storage.SearchResult
import mozilla.components.feature.session.SessionUseCases
import java.util.UUID

internal const val HISTORY_SUGGESTION_LIMIT = 20

/**
 * A [AwesomeBar.SuggestionProvider] implementation that provides suggestions based on the browsing
 * history stored in the [HistoryStorage].
 *
 * @property historyStorage and instance of the [HistoryStorage] used
 * to query matching bookmarks.
 * @property loadUrlUseCase the use case invoked to load the url when the
 * user clicks on the suggestion.
 * @property icons optional instance of [BrowserIcons] to load fav icons
 * for bookmarked URLs.
 * @param engine optional [Engine] instance to call [Engine.speculativeConnect] for the
 * highest scored suggestion URL.
 */
class HistoryStorageSuggestionProvider(
    private val historyStorage: HistoryStorage,
    private val loadUrlUseCase: SessionUseCases.LoadUrlUseCase,
    private val icons: BrowserIcons? = null,
    internal val engine: Engine? = null
) : AwesomeBar.SuggestionProvider {

    override val id: String = UUID.randomUUID().toString()

    override suspend fun onInputChanged(text: String): List<AwesomeBar.Suggestion> {
        if (text.isEmpty()) {
            return emptyList()
        }

        // In case of duplicates we want to pick the suggestion with the highest score.
        // See: https://github.com/mozilla/application-services/issues/970
        val suggestions = historyStorage.getSuggestions(text, HISTORY_SUGGESTION_LIMIT)
            .sortedByDescending { it.score }
            .distinctBy { it.id }

        suggestions.firstOrNull()?.url?.let { url -> engine?.speculativeConnect(url) }

        return suggestions.into()
    }

    override val shouldClearSuggestions: Boolean
        // We do not want the suggestion of this provider to disappear and re-appear when text changes.
        get() = false

    private suspend fun Iterable<SearchResult>.into(): List<AwesomeBar.Suggestion> {
        val iconRequests = this.map { icons?.loadIcon(IconRequest(it.url)) }
        return this.zip(iconRequests) { result, icon ->
            AwesomeBar.Suggestion(
                provider = this@HistoryStorageSuggestionProvider,
                id = result.id,
                icon = icon?.await()?.bitmap,
                title = result.title,
                description = result.url,
                editSuggestion = result.url,
                score = result.score,
                onSuggestionClicked = { loadUrlUseCase.invoke(result.url) }
            )
        }
    }
}
