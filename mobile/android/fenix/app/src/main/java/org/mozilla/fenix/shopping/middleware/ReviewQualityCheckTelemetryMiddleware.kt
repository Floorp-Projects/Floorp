/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.shopping.middleware

import mozilla.components.lib.state.MiddlewareContext
import mozilla.components.lib.state.Store
import org.mozilla.fenix.GleanMetrics.Shopping
import org.mozilla.fenix.GleanMetrics.ShoppingSettings
import org.mozilla.fenix.shopping.store.ReviewQualityCheckAction
import org.mozilla.fenix.shopping.store.ReviewQualityCheckMiddleware
import org.mozilla.fenix.shopping.store.ReviewQualityCheckState

/**
 * Middleware that captures telemetry events for the review quality check feature.
 */
class ReviewQualityCheckTelemetryMiddleware : ReviewQualityCheckMiddleware {

    override fun invoke(
        context: MiddlewareContext<ReviewQualityCheckState, ReviewQualityCheckAction>,
        next: (ReviewQualityCheckAction) -> Unit,
        action: ReviewQualityCheckAction,
    ) {
        next(action)

        when (action) {
            is ReviewQualityCheckAction.TelemetryAction -> processAction(context.store, action)
            else -> {
                // no-op
            }
        }
    }

    private fun processAction(
        store: Store<ReviewQualityCheckState, ReviewQualityCheckAction>,
        action: ReviewQualityCheckAction.TelemetryAction,
    ) {
        when (action) {
            is ReviewQualityCheckAction.OptIn -> {
                Shopping.surfaceOptInAccepted.record()
                ShoppingSettings.userHasOnboarded.set(true)
            }

            is ReviewQualityCheckAction.OptOut -> {
                ShoppingSettings.componentOptedOut.set(true)
            }

            is ReviewQualityCheckAction.BottomSheetClosed -> {
                Shopping.surfaceClosed.record(
                    Shopping.SurfaceClosedExtra(action.source.sourceName),
                )
            }

            is ReviewQualityCheckAction.BottomSheetDisplayed -> {
                Shopping.surfaceDisplayed.record(
                    Shopping.SurfaceDisplayedExtra(action.view.state),
                )
            }

            is ReviewQualityCheckAction.OpenExplainerLearnMoreLink -> {
                Shopping.surfaceReviewQualityExplainerUrlClicked.record()
            }

            is ReviewQualityCheckAction.OpenOnboardingLearnMoreLink -> {
                Shopping.surfaceLearnMoreClicked.record()
            }

            is ReviewQualityCheckAction.OpenOnboardingPrivacyPolicyLink -> {
                Shopping.surfaceShowPrivacyPolicyClicked.record()
            }

            is ReviewQualityCheckAction.OpenOnboardingTermsLink -> {
                Shopping.surfaceShowTermsClicked.record()
            }

            is ReviewQualityCheckAction.NotNowClicked -> {
                Shopping.surfaceNotNowClicked.record()
            }

            is ReviewQualityCheckAction.ExpandCollapseHighlights -> {
                val state = store.state
                if (state is ReviewQualityCheckState.OptedIn && state.isHighlightsExpanded) {
                    Shopping.surfaceShowMoreRecentReviewsClicked.record()
                }
            }

            is ReviewQualityCheckAction.ExpandCollapseSettings -> {
                val state = store.state
                if (state is ReviewQualityCheckState.OptedIn && state.isSettingsExpanded) {
                    Shopping.surfaceExpandSettings.record()
                }
            }

            is ReviewQualityCheckAction.NoAnalysisDisplayed -> {
                Shopping.surfaceNoReviewReliabilityAvailable.record()
            }

            is ReviewQualityCheckAction.AnalyzeProduct -> {
                Shopping.surfaceAnalyzeReviewsNoneAvailableClicked.record()
            }

            is ReviewQualityCheckAction.ReanalyzeProduct -> {
                Shopping.surfaceReanalyzeClicked.record()
            }

            is ReviewQualityCheckAction.ReportProductBackInStock -> {
                Shopping.surfaceReactivatedButtonClicked.record()
            }

            is ReviewQualityCheckAction.OptOutCompleted -> {
                Shopping.surfaceOnboardingDisplayed.record()
            }

            is ReviewQualityCheckAction.OpenPoweredByLink -> {
                Shopping.surfacePoweredByFakespotLinkClicked.record()
            }
        }
    }
}
