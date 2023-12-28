/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.shopping.middleware

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.launch
import mozilla.components.browser.state.selector.selectedTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.lib.state.MiddlewareContext
import mozilla.components.lib.state.Store
import org.mozilla.fenix.GleanMetrics.Shopping
import org.mozilla.fenix.GleanMetrics.ShoppingSettings
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.appstate.AppAction.ShoppingAction
import org.mozilla.fenix.components.appstate.shopping.ShoppingState
import org.mozilla.fenix.shopping.store.ReviewQualityCheckAction
import org.mozilla.fenix.shopping.store.ReviewQualityCheckMiddleware
import org.mozilla.fenix.shopping.store.ReviewQualityCheckState
import org.mozilla.fenix.shopping.store.ReviewQualityCheckState.OptedIn.ProductReviewState.AnalysisPresent.AnalysisStatus

private const val ACTION_ENABLED = "enabled"
private const val ACTION_DISABLED = "disabled"

/**
 * Middleware that captures telemetry events for the review quality check feature.
 *
 * @param telemetryService The service that handles telemetry events for review checker.
 * @param browserStore The [BrowserStore] instance to access the current tab.
 * @param appStore The [AppStore] instance to access [ShoppingState].
 * @param scope The [CoroutineScope] to use for launching coroutines.
 */
class ReviewQualityCheckTelemetryMiddleware(
    private val telemetryService: ReviewQualityCheckTelemetryService,
    private val browserStore: BrowserStore,
    private val appStore: AppStore,
    private val scope: CoroutineScope,
) : ReviewQualityCheckMiddleware {

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

    @Suppress("LongMethod")
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

            is ReviewQualityCheckAction.UpdateProductReview -> {
                val state = store.state
                if (state.isStaleAnalysis() && !action.restoreAnalysis) {
                    Shopping.surfaceStaleAnalysisShown.record()
                }
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

            is ReviewQualityCheckAction.RecommendedProductImpression -> {
                browserStore.state.selectedTab?.let { tabSessionState ->
                    val key = ShoppingState.ProductRecommendationImpressionKey(
                        tabId = tabSessionState.id,
                        productUrl = tabSessionState.content.url,
                        aid = action.productAid,
                    )

                    val recordedImpressions =
                        appStore.state.shoppingState.recordedProductRecommendationImpressions

                    if (!recordedImpressions.contains(key)) {
                        Shopping.surfaceAdsImpression.record()
                        scope.launch {
                            val result =
                                telemetryService.recordRecommendedProductImpression(action.productAid)
                            if (result != null) {
                                appStore.dispatch(ShoppingAction.ProductRecommendationImpression(key))
                            }
                        }
                    }
                }
            }

            is ReviewQualityCheckAction.RecommendedProductClick -> {
                Shopping.surfaceAdsClicked.record()
                scope.launch {
                    telemetryService.recordRecommendedProductClick(action.productAid)
                }
            }

            ReviewQualityCheckAction.ToggleProductRecommendation -> {
                val state = store.state
                if (state is ReviewQualityCheckState.OptedIn &&
                    state.productRecommendationsPreference != null
                ) {
                    val toggleAction = if (state.productRecommendationsPreference) {
                        ACTION_ENABLED
                    } else {
                        ACTION_DISABLED
                    }
                    Shopping.surfaceAdsSettingToggled.record(
                        Shopping.SurfaceAdsSettingToggledExtra(
                            action = toggleAction,
                        ),
                    )
                }
            }
        }
    }

    private fun ReviewQualityCheckState.isStaleAnalysis(): Boolean =
        this is ReviewQualityCheckState.OptedIn &&
            this.productReviewState is ReviewQualityCheckState.OptedIn.ProductReviewState.AnalysisPresent &&
            this.productReviewState.analysisStatus == AnalysisStatus.NeedsAnalysis
}
