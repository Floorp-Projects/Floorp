/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.shopping.ui

import androidx.compose.animation.Crossfade
import androidx.compose.animation.animateContentSize
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.ui.Modifier
import mozilla.components.lib.state.ext.observeAsState
import org.mozilla.fenix.shopping.state.ReviewQualityCheckAction
import org.mozilla.fenix.shopping.state.ReviewQualityCheckState
import org.mozilla.fenix.shopping.state.ReviewQualityCheckState.OptedIn.ProductReviewState.AnalysisPresent
import org.mozilla.fenix.shopping.state.ReviewQualityCheckStore

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

    ReviewQualityCheckScaffold(
        onRequestDismiss = onRequestDismiss,
        modifier = modifier.animateContentSize(),
    ) {
        when (val state = reviewQualityCheckState) {
            is ReviewQualityCheckState.NotOptedIn -> {
                ReviewQualityCheckContextualOnboarding(
                    onPrimaryButtonClick = {
                        store.dispatch(ReviewQualityCheckAction.OptIn)
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
                    onProductRecommendationsEnabledStateChange = {
                        store.dispatch(ReviewQualityCheckAction.ToggleProductRecommendation)
                    },
                )
            }

            is ReviewQualityCheckState.Initial -> {}
        }
    }
}

@Composable
private fun ProductReview(
    state: ReviewQualityCheckState.OptedIn,
    onOptOutClick: () -> Unit,
    onProductRecommendationsEnabledStateChange: (Boolean) -> Unit,
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
                    onOptOutClick = onOptOutClick,
                    onProductRecommendationsEnabledStateChange = onProductRecommendationsEnabledStateChange,
                )
            }

            is ReviewQualityCheckState.OptedIn.ProductReviewState.Error -> {
                // Bug 1840113
            }

            is ReviewQualityCheckState.OptedIn.ProductReviewState.Loading -> {
                ProductReviewLoading()
            }

            is ReviewQualityCheckState.OptedIn.ProductReviewState.NoAnalysisPresent -> {
                // Bug 1840333
            }
        }
    }
}
