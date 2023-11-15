/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.fxsuggest

import android.content.res.Resources
import mozilla.appservices.suggest.Suggestion
import mozilla.appservices.suggest.SuggestionProvider
import mozilla.appservices.suggest.SuggestionQuery
import mozilla.components.concept.awesomebar.AwesomeBar
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
    /**
     * [AwesomeBar.Suggestion.metadata] keys for this provider's suggestions.
     */
    object MetadataKeys {
        const val CLICK_INFO = "click_info"
        const val IMPRESSION_INFO = "impression_info"
    }

    override val id: String = UUID.randomUUID().toString()

    override fun groupTitle(): String? = suggestionsHeader

    override suspend fun onInputChanged(text: String): List<AwesomeBar.Suggestion> =
        if (text.isEmpty()) {
            emptyList()
        } else {
            val providers = buildList() {
                val availableSuggestionTypes = FxSuggestNimbus.features
                    .awesomebarSuggestionProvider
                    .value()
                    .availableSuggestionTypes
                if (includeSponsoredSuggestions && availableSuggestionTypes[SuggestionType.AMP] == true) {
                    add(SuggestionProvider.AMP)
                }
                if (includeNonSponsoredSuggestions && availableSuggestionTypes[SuggestionType.WIKIPEDIA] == true) {
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
                        FxSuggestInteractionInfo.Amp(
                            blockId = suggestion.blockId,
                            advertiser = suggestion.advertiser.lowercase(),
                            reportingUrl = suggestion.clickUrl,
                            iabCategory = suggestion.iabCategory,
                            contextId = it,
                        )
                    },
                    impressionInfo = contextId?.let {
                        FxSuggestInteractionInfo.Amp(
                            blockId = suggestion.blockId,
                            advertiser = suggestion.advertiser.lowercase(),
                            reportingUrl = suggestion.impressionUrl,
                            iabCategory = suggestion.iabCategory,
                            contextId = it,
                        )
                    },
                )
                is Suggestion.Wikipedia -> {
                    val interactionInfo = contextId?.let {
                        FxSuggestInteractionInfo.Wikipedia(contextId = it)
                    }
                    SuggestionDetails(
                        title = suggestion.title,
                        url = suggestion.url,
                        fullKeyword = suggestion.fullKeyword,
                        isSponsored = false,
                        icon = suggestion.icon,
                        clickInfo = interactionInfo,
                        impressionInfo = interactionInfo,
                    )
                }
                else -> return@mapNotNull null
            }
            AwesomeBar.Suggestion(
                provider = this@FxSuggestSuggestionProvider,
                icon = details.icon?.toUByteArray()?.asByteArray()?.toBitmap(),
                title = details.title,
                description = if (details.isSponsored) {
                    resources.getString(R.string.sponsored_suggestion_description)
                } else {
                    null
                },
                onSuggestionClicked = {
                    loadUrlUseCase.invoke(details.url)
                },
                score = Int.MIN_VALUE,
                metadata = buildMap {
                    details.clickInfo?.let { put(MetadataKeys.CLICK_INFO, it) }
                    details.impressionInfo?.let { put(MetadataKeys.IMPRESSION_INFO, it) }
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
    val clickInfo: FxSuggestInteractionInfo? = null,
    val impressionInfo: FxSuggestInteractionInfo? = null,
)

/**
 * Additional information about a Firefox Suggest [AwesomeBar.Suggestion] to record in telemetry when the user
 * interacts with the suggestion.
 */
sealed interface FxSuggestInteractionInfo {
    /**
     * Interaction information for a sponsored Firefox Suggest search suggestion from AMP.
     *
     * @param blockId A unique identifier for the suggestion.
     * @param advertiser The name of the advertiser providing the sponsored suggestion.
     * @param reportingUrl The url to report the click or impression to.
     * @param iabCategory The categorization of the suggestion.
     * @param contextId The contextual services user identifier.
     */
    data class Amp(
        val blockId: Long,
        val advertiser: String,
        val reportingUrl: String,
        val iabCategory: String,
        val contextId: String,
    ) : FxSuggestInteractionInfo

    /**
     * Interaction information for a Firefox Suggest search suggestion from Wikipedia.
     *
     * @param contextId The contextual services user identifier.
     */
    data class Wikipedia(
        val contextId: String,
    ) : FxSuggestInteractionInfo
}
