/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.awesomebar.provider

import androidx.annotation.VisibleForTesting
import mozilla.components.browser.icons.BrowserIcons
import mozilla.components.browser.icons.IconRequest
import mozilla.components.concept.awesomebar.AwesomeBar
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.storage.HistoryMetadata
import mozilla.components.concept.storage.HistoryMetadataStorage
import mozilla.components.concept.storage.HistoryStorage
import mozilla.components.feature.awesomebar.facts.emitHistorySuggestionClickedFact
import mozilla.components.feature.session.SessionUseCases
import java.util.UUID

/**
 * Return 5 history suggestions by default.
 */
const val DEFAULT_METADATA_SUGGESTION_LIMIT = 5

/**
 * A [AwesomeBar.SuggestionProvider] implementation that provides suggestions based on [HistoryMetadata].
 *
 * @param historyStorage an instance of the [HistoryStorage] used
 * to query matching metadata records.
 * @param loadUrlUseCase the use case invoked to load the url when the
 * user clicks on the suggestion.
 * @param icons optional instance of [BrowserIcons] to load fav icons
 * for [HistoryMetadata] URLs.
 * @param engine optional [Engine] instance to call [Engine.speculativeConnect] for the
 * highest scored suggestion URL.
 * @param maxNumberOfSuggestions optional parameter to specify the maximum number of returned suggestions,
 * defaults to [DEFAULT_METADATA_SUGGESTION_LIMIT].
 * @param showEditSuggestion optional parameter to specify if the suggestion should show the edit button
 * @param suggestionsHeader optional parameter to specify if the suggestion should have a header
 */
class HistoryMetadataSuggestionProvider(
    @get:VisibleForTesting internal val historyStorage: HistoryMetadataStorage,
    private val loadUrlUseCase: SessionUseCases.LoadUrlUseCase,
    private val icons: BrowserIcons? = null,
    internal val engine: Engine? = null,
    @get:VisibleForTesting internal val maxNumberOfSuggestions: Int = DEFAULT_METADATA_SUGGESTION_LIMIT,
    private val showEditSuggestion: Boolean = true,
    private val suggestionsHeader: String? = null,
) : AwesomeBar.SuggestionProvider {
    override val id: String = UUID.randomUUID().toString()

    override fun groupTitle(): String? {
        return suggestionsHeader
    }

    override suspend fun onInputChanged(text: String): List<AwesomeBar.Suggestion> {
        historyStorage.cancelReads()

        if (text.isNullOrBlank()) {
            return emptyList()
        }

        val suggestions = historyStorage
            .queryHistoryMetadata(text, maxNumberOfSuggestions)
            .filter { it.totalViewTime > 0 }

        suggestions.firstOrNull()?.key?.url?.let { url -> engine?.speculativeConnect(url) }
        return suggestions.into(this, icons, loadUrlUseCase, showEditSuggestion)
    }
}

internal suspend fun Iterable<HistoryMetadata>.into(
    provider: AwesomeBar.SuggestionProvider,
    icons: BrowserIcons?,
    loadUrlUseCase: SessionUseCases.LoadUrlUseCase,
    showEditSuggestion: Boolean = true,
): List<AwesomeBar.Suggestion> {
    val iconRequests = this.map { icons?.loadIcon(IconRequest(url = it.key.url, waitOnNetworkLoad = false)) }
    return this.zip(iconRequests) { result, icon ->
        AwesomeBar.Suggestion(
            provider = provider,
            icon = icon?.await()?.bitmap,
            title = result.title,
            description = result.key.url,
            editSuggestion = if (showEditSuggestion) result.key.url else null,
            onSuggestionClicked = {
                loadUrlUseCase.invoke(result.key.url)
                emitHistorySuggestionClickedFact()
            },
        )
    }
}
