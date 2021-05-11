/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.awesomebar.provider

import mozilla.components.browser.icons.BrowserIcons
import mozilla.components.browser.icons.IconRequest
import mozilla.components.concept.awesomebar.AwesomeBar
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.storage.HistoryMetadata
import mozilla.components.concept.storage.HistoryMetadataStorage
import mozilla.components.concept.storage.HistoryStorage
import mozilla.components.feature.session.SessionUseCases
import java.util.UUID

internal const val METADATA_SUGGESTION_LIMIT = 5

/**
 * A [AwesomeBar.SuggestionProvider] implementation that provides suggestions based on [HistoryMetadata].
 *
 * @property historyStorage an instance of the [HistoryStorage] used
 * to query matching metadata records.
 * @property loadUrlUseCase the use case invoked to load the url when the
 * user clicks on the suggestion.
 * @property icons optional instance of [BrowserIcons] to load fav icons
 * for [HistoryMetadata] URLs.
 * @param engine optional [Engine] instance to call [Engine.speculativeConnect] for the
 * highest scored suggestion URL.
 */
class HistoryMetadataSuggestionProvider(
    private val historyStorage: HistoryMetadataStorage,
    private val loadUrlUseCase: SessionUseCases.LoadUrlUseCase,
    private val icons: BrowserIcons? = null,
    internal val engine: Engine? = null
) : AwesomeBar.SuggestionProvider {
    override val id: String = UUID.randomUUID().toString()

    override val shouldClearSuggestions: Boolean
        get() = false

    override suspend fun onInputChanged(text: String): List<AwesomeBar.Suggestion> {
        if (text.isNullOrBlank()) {
            return emptyList()
        }

        val suggestions = historyStorage
            .queryHistoryMetadata(text, METADATA_SUGGESTION_LIMIT)

        suggestions.firstOrNull()?.url?.let { url -> engine?.speculativeConnect(url) }
        return suggestions.into()
    }

    private suspend fun Iterable<HistoryMetadata>.into(): List<AwesomeBar.Suggestion> {
        val iconRequests = this.map { icons?.loadIcon(IconRequest(it.url)) }
        return this.zip(iconRequests) { result, icon ->
            AwesomeBar.Suggestion(
                provider = this@HistoryMetadataSuggestionProvider,
                id = result.guid ?: "",
                icon = icon?.await()?.bitmap,
                title = result.title,
                description = result.url,
                editSuggestion = result.url,
                onSuggestionClicked = {
                    loadUrlUseCase.invoke(result.url)
                }
            )
        }
    }
}
