/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.fxsuggest.facts

import mozilla.components.browser.state.action.AwesomeBarAction
import mozilla.components.browser.state.action.BrowserAction
import mozilla.components.browser.state.state.AwesomeBarState
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.concept.awesomebar.AwesomeBar
import mozilla.components.feature.fxsuggest.FxSuggestInteractionInfo
import mozilla.components.feature.fxsuggest.FxSuggestSuggestionProvider
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext
import mozilla.components.support.base.facts.Fact

/**
 * Reports [Fact]s for interactions with Firefox Suggest [AwesomeBar.Suggestion]s.
 *
 * We report two kinds of interactions: impressions and clicks. We report impressions for any Firefox Suggest
 * search suggestions that are still visible just before the user navigates to a destination: either a URL, a
 * search results page, or a suggestion. If the user navigates to one of those Firefox Suggest suggestions,
 * we also report a click.
 *
 * We _don't_ report impressions for any suggestions that the user sees as they're still typing, or
 * if they dismiss the [AwesomeBar] without navigating to a destination.
 */
class FxSuggestFactsMiddleware : Middleware<BrowserState, BrowserAction> {
    override fun invoke(
        context: MiddlewareContext<BrowserState, BrowserAction>,
        next: (BrowserAction) -> Unit,
        action: BrowserAction,
    ) {
        handleAction(context, action)
        next(action)
    }

    private fun handleAction(
        context: MiddlewareContext<BrowserState, BrowserAction>,
        action: BrowserAction,
    ) = when (action) {
        is AwesomeBarAction.EngagementFinished ->
            if (action.abandoned) Unit else emitSuggestionFacts(context.state.awesomeBarState)
        else -> Unit
    }

    private fun emitSuggestionFacts(awesomeBarState: AwesomeBarState) {
        val visibilityState = awesomeBarState.visibilityState
        val clickedSuggestion = awesomeBarState.clickedSuggestion
        visibilityState.visibleProviderGroups.entries.forEachIndexed { groupIndex, (_, suggestions) ->
            suggestions.forEachIndexed { suggestionIndex, suggestion ->
                val positionInGroup = suggestionIndex.toLong() + 1
                val positionInAwesomeBar = groupIndex.toLong() + positionInGroup
                val isClicked = clickedSuggestion == suggestion

                val impressionInfo = suggestion.metadata?.get(
                    FxSuggestSuggestionProvider.MetadataKeys.IMPRESSION_INFO,
                ) as? FxSuggestInteractionInfo
                impressionInfo?.let {
                    emitSuggestionImpressedFact(it, positionInGroup, isClicked = isClicked)
                }

                if (isClicked) {
                    val clickInfo = suggestion.metadata?.get(
                        FxSuggestSuggestionProvider.MetadataKeys.CLICK_INFO,
                    ) as? FxSuggestInteractionInfo
                    clickInfo?.let {
                        emitSuggestionClickedFact(it, positionInAwesomeBar)
                    }
                }
            }
        }
    }
}
