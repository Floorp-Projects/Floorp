/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.shopping.middleware

import mozilla.components.feature.tabs.TabsUseCases
import mozilla.components.lib.state.MiddlewareContext
import org.mozilla.fenix.shopping.store.ReviewQualityCheckAction
import org.mozilla.fenix.shopping.store.ReviewQualityCheckMiddleware
import org.mozilla.fenix.shopping.store.ReviewQualityCheckState

private const val POWERED_BY_URL =
    "https://www.fakespot.com/review-checker?utm_source=review-checker" +
        "&utm_campaign=fakespot-by-mozilla&utm_medium=inproduct&utm_term=core-sheet"
private const val PRIVACY_POLICY_URL = "https://www.mozilla.org/en-US/privacy/firefox/#review-checker" +
    "?utm_source=review-checker&utm_campaign=privacy-policy&utm_medium=in-product&utm_term=opt-in-screen"
private const val TERMS_OF_USE_URL = "https://www.fakespot.com/terms"

/**
 * Middleware that handles navigation events for the review quality check feature.
 *
 * @param selectOrAddUseCase UseCase instance used to open new tabs.
 * @param getReviewQualityCheckSumoUrl Instance used to retrieve the learn more SUMO link.
 */
class ReviewQualityCheckNavigationMiddleware(
    private val selectOrAddUseCase: TabsUseCases.SelectOrAddUseCase,
    private val getReviewQualityCheckSumoUrl: GetReviewQualityCheckSumoUrl,
) : ReviewQualityCheckMiddleware {

    override fun invoke(
        context: MiddlewareContext<ReviewQualityCheckState, ReviewQualityCheckAction>,
        next: (ReviewQualityCheckAction) -> Unit,
        action: ReviewQualityCheckAction,
    ) {
        next(action)
        when (action) {
            is ReviewQualityCheckAction.NavigationMiddlewareAction -> processAction(action)
            else -> {
                // no-op
            }
        }
    }

    private fun processAction(
        action: ReviewQualityCheckAction.NavigationMiddlewareAction,
    ) {
        selectOrAddUseCase.invoke(actionToUrl(action))
    }

    /**
     * Used to find the corresponding url to the open link action.
     *
     * @param action Used to find the corresponding url.
     */
    private fun actionToUrl(
        action: ReviewQualityCheckAction.NavigationMiddlewareAction,
    ) = when (action) {
        is ReviewQualityCheckAction.OpenExplainerLearnMoreLink,
        ReviewQualityCheckAction.OpenOnboardingLearnMoreLink,
        -> getReviewQualityCheckSumoUrl()

        is ReviewQualityCheckAction.OpenOnboardingTermsLink -> TERMS_OF_USE_URL

        is ReviewQualityCheckAction.OpenOnboardingPrivacyPolicyLink -> PRIVACY_POLICY_URL

        is ReviewQualityCheckAction.OpenPoweredByLink -> POWERED_BY_URL

        is ReviewQualityCheckAction.RecommendedProductClick -> action.productUrl
    }
}
