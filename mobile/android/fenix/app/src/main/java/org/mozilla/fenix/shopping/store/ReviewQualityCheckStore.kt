/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.shopping.store

import mozilla.components.lib.state.Store
import org.mozilla.fenix.shopping.store.ReviewQualityCheckState.OptedIn.ProductReviewState
import org.mozilla.fenix.shopping.store.ReviewQualityCheckState.OptedIn.ProductReviewState.AnalysisPresent.AnalysisStatus

/**
 * Store for review quality check feature.
 *
 * @param middleware The list of middlewares to use.
 */
class ReviewQualityCheckStore(
    middleware: List<ReviewQualityCheckMiddleware>,
) : Store<ReviewQualityCheckState, ReviewQualityCheckAction>(
    initialState = ReviewQualityCheckState.Initial,
    middleware = middleware,
    reducer = ::reducer,
) {
    init {
        dispatch(ReviewQualityCheckAction.Init)
    }
}

private fun reducer(
    state: ReviewQualityCheckState,
    action: ReviewQualityCheckAction,
): ReviewQualityCheckState {
    if (action is ReviewQualityCheckAction.UpdateAction) {
        return mapStateForUpdateAction(state, action)
    }

    return state
}

@Suppress("LongMethod")
private fun mapStateForUpdateAction(
    state: ReviewQualityCheckState,
    action: ReviewQualityCheckAction.UpdateAction,
): ReviewQualityCheckState {
    return when (action) {
        is ReviewQualityCheckAction.OptInCompleted -> {
            if (state is ReviewQualityCheckState.OptedIn) {
                state.copy(productRecommendationsPreference = action.isProductRecommendationsEnabled)
            } else {
                ReviewQualityCheckState.OptedIn(
                    productRecommendationsPreference = action.isProductRecommendationsEnabled,
                    productVendor = action.productVendor,
                )
            }
        }

        is ReviewQualityCheckAction.OptOutCompleted -> {
            ReviewQualityCheckState.NotOptedIn(action.productVendors)
        }

        ReviewQualityCheckAction.OptOut -> {
            ReviewQualityCheckState.NotOptedIn()
        }

        ReviewQualityCheckAction.ToggleProductRecommendation -> {
            if (state is ReviewQualityCheckState.OptedIn && state.productRecommendationsPreference != null) {
                if (state.productReviewState is ProductReviewState.AnalysisPresent &&
                    state.productRecommendationsPreference
                ) {
                    // Removes any existing product recommendation from UI
                    state.copy(
                        productRecommendationsPreference = false,
                        productReviewState = state.productReviewState.copy(
                            recommendedProductState = ReviewQualityCheckState.RecommendedProductState.Initial,
                        ),
                    )
                } else {
                    state.copy(productRecommendationsPreference = !state.productRecommendationsPreference)
                }
            } else {
                state
            }
        }

        is ReviewQualityCheckAction.UpdateProductReview -> {
            state.mapIfOptedIn {
                it.copy(productReviewState = action.productReviewState)
            }
        }

        ReviewQualityCheckAction.FetchProductAnalysis, ReviewQualityCheckAction.RetryProductAnalysis -> {
            state.mapIfOptedIn {
                it.copy(productReviewState = ProductReviewState.Loading)
            }
        }

        ReviewQualityCheckAction.ReanalyzeProduct, ReviewQualityCheckAction.AnalyzeProduct -> {
            state.mapIfOptedIn {
                when (it.productReviewState) {
                    is ProductReviewState.AnalysisPresent -> {
                        val productReviewState =
                            it.productReviewState.copy(analysisStatus = AnalysisStatus.REANALYZING)
                        it.copy(productReviewState = productReviewState)
                    }

                    is ProductReviewState.NoAnalysisPresent -> {
                        it.copy(productReviewState = it.productReviewState.copy(isReanalyzing = true))
                    }

                    else -> {
                        it
                    }
                }
            }
        }

        is ReviewQualityCheckAction.UpdateRecommendedProduct -> {
            state.mapIfOptedIn {
                if (it.productReviewState is ProductReviewState.AnalysisPresent &&
                    it.productRecommendationsPreference == true
                ) {
                    it.copy(
                        productReviewState = it.productReviewState.copy(
                            recommendedProductState = action.recommendedProductState,
                        ),
                    )
                } else {
                    it
                }
            }
        }
    }
}
