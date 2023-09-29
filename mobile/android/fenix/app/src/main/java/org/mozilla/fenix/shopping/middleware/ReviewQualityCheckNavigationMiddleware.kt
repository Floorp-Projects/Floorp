/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.shopping.middleware

import android.content.Context
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.launch
import mozilla.components.feature.tabs.TabsUseCases
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext
import org.mozilla.fenix.settings.SupportUtils
import org.mozilla.fenix.shopping.store.ReviewQualityCheckAction
import org.mozilla.fenix.shopping.store.ReviewQualityCheckState

private const val POWERED_BY_URL =
    "https://www.fakespot.com/review-checker?utm_source=review-checker" +
        "&utm_campaign=fakespot-by-mozilla&utm_medium=inproduct&utm_term=core-sheet"

/**
 * Middleware that handles navigation events for the review quality check feature.
 *
 * @property selectOrAddUseCase UseCase instance used to open new tabs.
 * @property context Context used to get SUMO urls.
 * @property scope [CoroutineScope] used to launch coroutines.
 */
class ReviewQualityCheckNavigationMiddleware(
    private val selectOrAddUseCase: TabsUseCases.SelectOrAddUseCase,
    private val context: Context,
    private val scope: CoroutineScope,
) : Middleware<ReviewQualityCheckState, ReviewQualityCheckAction> {

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
    ) = scope.launch {
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
        // Placeholder SUMO urls to be used until the Fakespot SUMO pages are added in 1854277
        is ReviewQualityCheckAction.OpenExplainerLearnMoreLink -> SupportUtils.getSumoURLForTopic(
            context,
            SupportUtils.SumoTopic.HELP,
        )

        is ReviewQualityCheckAction.OpenOnboardingTermsLink -> SupportUtils.getSumoURLForTopic(
            context,
            SupportUtils.SumoTopic.HELP,
        )

        is ReviewQualityCheckAction.OpenOnboardingLearnMoreLink -> SupportUtils.getSumoURLForTopic(
            context,
            SupportUtils.SumoTopic.HELP,
        )

        is ReviewQualityCheckAction.OpenOnboardingPrivacyPolicyLink -> SupportUtils.getSumoURLForTopic(
            context,
            SupportUtils.SumoTopic.HELP,
        )

        is ReviewQualityCheckAction.OpenPoweredByLink -> POWERED_BY_URL
    }
}
