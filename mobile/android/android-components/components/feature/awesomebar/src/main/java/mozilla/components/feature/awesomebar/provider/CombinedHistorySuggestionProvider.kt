/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.awesomebar.provider

import androidx.annotation.VisibleForTesting
import kotlinx.coroutines.async
import kotlinx.coroutines.coroutineScope
import mozilla.components.browser.icons.BrowserIcons
import mozilla.components.concept.awesomebar.AwesomeBar
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.storage.HistoryMetadata
import mozilla.components.concept.storage.HistoryMetadataStorage
import mozilla.components.concept.storage.HistoryStorage
import mozilla.components.feature.session.SessionUseCases
import java.util.UUID

/**
 * Return 5 history suggestions by default.
 */
const val DEFAULT_COMBINED_SUGGESTION_LIMIT = 5

/**
 * A [AwesomeBar.SuggestionProvider] implementation that combines suggestions from
 * [HistoryMetadataSuggestionProvider] and [HistoryStorageSuggestionProvider].
 * It will return suggestions using [HistoryMetadataSuggestionProvider] first,
 * followed by suggestion from [HistoryStorageSuggestionProvider] up to the provided
 * [maxNumberOfSuggestions].
 *
 * @param historyStorage an instance of the [HistoryStorage] used
 * to query matching metadata records.
 * @param historyMetadataStorage an instance of the [HistoryStorage] used
 * to query matching metadata records.
 * @param loadUrlUseCase the use case invoked to load the url when the
 * user clicks on the suggestion.
 * @param icons optional instance of [BrowserIcons] to load fav icons
 * for [HistoryMetadata] URLs.
 * @param engine optional [Engine] instance to call [Engine.speculativeConnect] for the
 * highest scored suggestion URL.
 * @param maxNumberOfSuggestions optional parameter to specify the maximum number of returned suggestions,
 * defaults to [DEFAULT_COMBINED_SUGGESTION_LIMIT].
 */
@Suppress("LongParameterList")
class CombinedHistorySuggestionProvider(
    private val historyStorage: HistoryStorage,
    private val historyMetadataStorage: HistoryMetadataStorage,
    private val loadUrlUseCase: SessionUseCases.LoadUrlUseCase,
    private val icons: BrowserIcons? = null,
    internal val engine: Engine? = null,
    @VisibleForTesting internal val maxNumberOfSuggestions: Int = DEFAULT_COMBINED_SUGGESTION_LIMIT
) : AwesomeBar.SuggestionProvider {
    override val id: String = UUID.randomUUID().toString()

    override suspend fun onInputChanged(text: String): List<AwesomeBar.Suggestion> = coroutineScope {
        if (text.isBlank()) {
            return@coroutineScope emptyList()
        }

        // Do both queries in parallel to optimize for speed.
        val metadataSuggestions = async {
            historyMetadataStorage
                .queryHistoryMetadata(text, maxNumberOfSuggestions)
                .filter { it.totalViewTime > 0 }
                .into(this@CombinedHistorySuggestionProvider, icons, loadUrlUseCase)
        }
        val historySuggestions = async {
            historyStorage.getSuggestions(text, maxNumberOfSuggestions)
                .sortedByDescending { it.score }
                .distinctBy { it.id }
                .into(this@CombinedHistorySuggestionProvider, icons, loadUrlUseCase)
        }

        val combinedSuggestions = (metadataSuggestions.await() + historySuggestions.await())
            .distinctBy { it.description }
            .take(maxNumberOfSuggestions)
        combinedSuggestions.firstOrNull()?.description?.let { url -> engine?.speculativeConnect(url) }

        return@coroutineScope combinedSuggestions
    }
}
