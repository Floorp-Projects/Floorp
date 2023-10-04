/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.fxsuggest

import android.content.res.Resources
import mozilla.appservices.suggest.Suggestion
import mozilla.appservices.suggest.SuggestionProvider
import mozilla.appservices.suggest.SuggestionQuery
import mozilla.components.concept.awesomebar.AwesomeBar
import mozilla.components.feature.fxsuggest.facts.emitSponsoredSuggestionClickedFact
import mozilla.components.feature.session.SessionUseCases
import mozilla.components.support.ktx.kotlin.toBitmap
import java.util.UUID

/**
 * An [AwesomeBar.SuggestionProvider] that returns Firefox Suggest search suggestions.
 *
 * @param resources Your application's [Resources] instance.
 * @param loadUrlUseCase A use case that loads a suggestion's URL when clicked.
 * @param includeSponsoredSuggestions Whether to return suggestions for sponsored content.
 * @param includeNonSponsoredSuggestions Whether to return suggestions for web content.
 * @param suggestionsHeader An optional header title for grouping the returned suggestions.
 * @param contextId The contextual services user identifier, used for telemetry.
 */
class FxSuggestSuggestionProvider(
    private val resources: Resources,
    private val loadUrlUseCase: SessionUseCases.LoadUrlUseCase,
    private val includeSponsoredSuggestions: Boolean,
    private val includeNonSponsoredSuggestions: Boolean,
    private val suggestionsHeader: String? = null,
    private val contextId: String? = null,
) : AwesomeBar.SuggestionProvider {
    override val id: String = UUID.randomUUID().toString()

    override fun groupTitle(): String? = suggestionsHeader

    override suspend fun onInputChanged(text: String): List<AwesomeBar.Suggestion> =
        if (text.isEmpty()) {
            emptyList()
        } else {
            val providers = buildList() {
                if (includeSponsoredSuggestions) {
                    add(SuggestionProvider.AMP)
                }
                if (includeNonSponsoredSuggestions) {
                    add(SuggestionProvider.WIKIPEDIA)
                }
            }
            GlobalFxSuggestDependencyProvider.requireStorage().query(
                SuggestionQuery(
                    keyword = text,
                    providers = providers,
                ),
            ).into()
        }

    override fun onInputCancelled() {
        GlobalFxSuggestDependencyProvider.requireStorage().cancelReads()
    }

    private suspend fun List<Suggestion>.into(): List<AwesomeBar.Suggestion> =
        mapNotNull { suggestion ->
            val details = when (suggestion) {
                is Suggestion.Amp -> SuggestionDetails(
                    title = suggestion.title,
                    url = suggestion.url,
                    fullKeyword = suggestion.fullKeyword,
                    isSponsored = true,
                    icon = suggestion.icon,
                    clickInfo = contextId?.let {
                        FxSuggestClickInfo.Amp(
                            blockId = suggestion.blockId,
                            advertiser = suggestion.advertiser.lowercase(),
                            clickUrl = suggestion.clickUrl,
                            iabCategory = suggestion.iabCategory,
                            contextId = it,
                        )
                    },
                )
                is Suggestion.Wikipedia -> SuggestionDetails(
                    title = suggestion.title,
                    url = suggestion.url,
                    fullKeyword = suggestion.fullKeyword,
                    isSponsored = false,
                    icon = suggestion.icon,
                )
                else -> return@mapNotNull null
            }
            AwesomeBar.Suggestion(
                provider = this@FxSuggestSuggestionProvider,
                icon = details.icon?.let {
                    it.toUByteArray().asByteArray().toBitmap()
                },
                title = details.title,
                description = if (details.isSponsored) {
                    resources.getString(R.string.sponsored_suggestion_description)
                } else {
                    null
                },
                onSuggestionClicked = {
                    loadUrlUseCase.invoke(details.url)
                    details.clickInfo?.let {
                        emitSponsoredSuggestionClickedFact(it)
                    }
                },
            )
        }
}

internal data class SuggestionDetails(
    val title: String,
    val url: String,
    val fullKeyword: String,
    val isSponsored: Boolean,
    val icon: List<UByte>?,
    val clickInfo: FxSuggestClickInfo? = null,
)

/**
 * Collective of fields required for fxsuggest click telemetry
 */
sealed interface FxSuggestClickInfo {
    /**
     * AMP related fields for fx suggest click telemetry
     *
     * @param blockId A unique identifier for the suggestion.
     * @param advertiser The name of the advertiser providing the sponsored suggestion.
     * @param clickUrl The url to report the click to.
     * @param iabCategory The categorization of the suggestion.
     * @param contextId The contextual services user identifier.
     */
    data class Amp(
        val blockId: Long,
        val advertiser: String,
        val clickUrl: String,
        val iabCategory: String,
        val contextId: String,
    ) : FxSuggestClickInfo
}
