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
 * @param maxNumberOfResults optional parameter allowing to lower the number of returned suggested
 * history items to below the default of 5
 */
class HistoryMetadataSuggestionProvider(
    private val historyStorage: HistoryMetadataStorage,
    private val loadUrlUseCase: SessionUseCases.LoadUrlUseCase,
    private val icons: BrowserIcons? = null,
    internal val engine: Engine? = null,
    @VisibleForTesting internal val maxNumberOfResults: Int = -1
) : AwesomeBar.SuggestionProvider {
    override val id: String = UUID.randomUUID().toString()

    override val shouldClearSuggestions: Boolean
        get() = false

    override suspend fun onInputChanged(text: String): List<AwesomeBar.Suggestion> {
        if (text.isNullOrBlank()) {
            return emptyList()
        }

        // Having both maxNumberOfResults and METADATA_SUGGESTION_LIMIT ensures that when asking for
        // a lower number of suggestions the below filtering will have some that can be dropped.
        val maxReturnedSuggestions = if (maxNumberOfResults > 0) {
            minOf(maxNumberOfResults, METADATA_SUGGESTION_LIMIT)
        } else {
            METADATA_SUGGESTION_LIMIT
        }

        val suggestions = historyStorage
            .queryHistoryMetadata(text, METADATA_SUGGESTION_LIMIT)
            .filter { it.totalViewTime > 0 }
            .take(maxReturnedSuggestions)

        suggestions.firstOrNull()?.key?.url?.let { url -> engine?.speculativeConnect(url) }
        return suggestions.into()
    }

    private suspend fun Iterable<HistoryMetadata>.into(): List<AwesomeBar.Suggestion> {
        val iconRequests = this.map { icons?.loadIcon(IconRequest(it.key.url)) }
        return this.zip(iconRequests) { result, icon ->
            AwesomeBar.Suggestion(
                provider = this@HistoryMetadataSuggestionProvider,
                icon = icon?.await()?.bitmap,
                title = result.title,
                description = result.key.url,
                editSuggestion = result.key.url,
                onSuggestionClicked = {
                    loadUrlUseCase.invoke(result.key.url)
                }
            )
        }
    }
}
