/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.shopping.ui

import androidx.compose.animation.Crossfade
import androidx.compose.animation.animateContentSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import mozilla.components.lib.state.ext.observeAsState
import org.mozilla.fenix.shopping.store.ReviewQualityCheckAction
import org.mozilla.fenix.shopping.store.ReviewQualityCheckState
import org.mozilla.fenix.shopping.store.ReviewQualityCheckState.OptedIn.ProductReviewState.AnalysisPresent
import org.mozilla.fenix.shopping.store.ReviewQualityCheckStore

/**
 * Top-level UI for the Review Quality Check feature.
 *
 * @param store [ReviewQualityCheckStore] that holds the state.
 * @param onRequestDismiss Invoked when a user action requests dismissal of the bottom sheet.
 * @param modifier The modifier to be applied to the Composable.
 */
@Composable
fun ReviewQualityCheckBottomSheet(
    store: ReviewQualityCheckStore,
    onRequestDismiss: () -> Unit,
    modifier: Modifier = Modifier,
) {
    val reviewQualityCheckState by store.observeAsState(ReviewQualityCheckState.Initial) { it }
    val isOptedIn =
        remember(reviewQualityCheckState) { reviewQualityCheckState is ReviewQualityCheckState.OptedIn }

    ReviewQualityCheckScaffold(
        onRequestDismiss = onRequestDismiss,
        modifier = modifier.animateContentSize(),
    ) {
        when (val state = reviewQualityCheckState) {
            is ReviewQualityCheckState.NotOptedIn -> {
                ReviewQualityCheckContextualOnboarding(
                    productVendors = state.productVendors,
                    onPrimaryButtonClick = {
                        store.dispatch(ReviewQualityCheckAction.OptIn)
                    },
                    onLearnMoreClick = {
                        onRequestDismiss()
                        store.dispatch(ReviewQualityCheckAction.OpenOnboardingLearnMoreLink)
                    },
                    onPrivacyPolicyClick = {
                        onRequestDismiss()
                        store.dispatch(ReviewQualityCheckAction.OpenOnboardingPrivacyPolicyLink)
                    },
                    onTermsOfUseClick = {
                        onRequestDismiss()
                        store.dispatch(ReviewQualityCheckAction.OpenOnboardingTermsLink)
                    },
                    onSecondaryButtonClick = onRequestDismiss,
                )
            }

            is ReviewQualityCheckState.OptedIn -> {
                ProductReview(
                    state = state,
                    onOptOutClick = {
                        onRequestDismiss()
                        store.dispatch(ReviewQualityCheckAction.OptOut)
                    },
                    onReanalyzeClick = {
                        store.dispatch(ReviewQualityCheckAction.ReanalyzeProduct)
                    },
                    onProductRecommendationsEnabledStateChange = {
                        store.dispatch(ReviewQualityCheckAction.ToggleProductRecommendation)
                    },
                    onReviewGradeLearnMoreClick = {
                        onRequestDismiss()
                        store.dispatch(ReviewQualityCheckAction.OpenExplainerLearnMoreLink)
                    },
                    onFooterLinkClick = {
                        onRequestDismiss()
                        store.dispatch(ReviewQualityCheckAction.OpenPoweredByLink)
                    },
                )
            }

            is ReviewQualityCheckState.Initial -> {}
        }
    }

    LaunchedEffect(isOptedIn) {
        if (isOptedIn) {
            store.dispatch(ReviewQualityCheckAction.FetchProductAnalysis)
        }
    }
}

@Composable
@Suppress("LongParameterList")
private fun ProductReview(
    state: ReviewQualityCheckState.OptedIn,
    onOptOutClick: () -> Unit,
    onReanalyzeClick: () -> Unit,
    onProductRecommendationsEnabledStateChange: (Boolean) -> Unit,
    onReviewGradeLearnMoreClick: () -> Unit,
    onFooterLinkClick: () -> Unit,
) {
    Crossfade(
        targetState = state.productReviewState,
        label = "ProductReview-Crossfade",
    ) { productReviewState ->
        when (productReviewState) {
            is AnalysisPresent -> {
                ProductAnalysis(
                    productRecommendationsEnabled = state.productRecommendationsPreference,
                    productAnalysis = productReviewState,
                    productVendor = state.productVendor,
                    onOptOutClick = onOptOutClick,
                    onReanalyzeClick = onReanalyzeClick,
                    onProductRecommendationsEnabledStateChange = onProductRecommendationsEnabledStateChange,
                    onReviewGradeLearnMoreClick = onReviewGradeLearnMoreClick,
                    onFooterLinkClick = onFooterLinkClick,
                )
            }

            is ReviewQualityCheckState.OptedIn.ProductReviewState.Error -> {
                ProductAnalysisError(
                    error = productReviewState,
                    productRecommendationsEnabled = state.productRecommendationsPreference,
                    productVendor = state.productVendor,
                    onReviewGradeLearnMoreClick = onReviewGradeLearnMoreClick,
                    onOptOutClick = onOptOutClick,
                    onProductRecommendationsEnabledStateChange = onProductRecommendationsEnabledStateChange,
                    onFooterLinkClick = onFooterLinkClick,
                    modifier = Modifier.fillMaxWidth(),
                )
            }

            is ReviewQualityCheckState.OptedIn.ProductReviewState.Loading -> {
                ProductReviewLoading()
            }

            is ReviewQualityCheckState.OptedIn.ProductReviewState.NoAnalysisPresent -> {
                NoAnalysis(
                    isAnalyzing = productReviewState.isReanalyzing,
                    productRecommendationsEnabled = state.productRecommendationsPreference,
                    productVendor = state.productVendor,
                    onAnalyzeClick = onReanalyzeClick,
                    onReviewGradeLearnMoreClick = onReviewGradeLearnMoreClick,
                    onOptOutClick = onOptOutClick,
                    onProductRecommendationsEnabledStateChange = onProductRecommendationsEnabledStateChange,
                    modifier = Modifier.fillMaxWidth(),
                )
            }
        }
    }
}
