/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.shopping.store

import mozilla.components.lib.state.Store

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

private fun mapStateForUpdateAction(
    state: ReviewQualityCheckState,
    action: ReviewQualityCheckAction.UpdateAction,
): ReviewQualityCheckState {
    return when (action) {
        is ReviewQualityCheckAction.UpdateUserPreferences -> {
            if (action.hasUserOptedIn) {
                if (state is ReviewQualityCheckState.OptedIn) {
                    state.copy(productRecommendationsPreference = action.isProductRecommendationsEnabled)
                } else {
                    ReviewQualityCheckState.OptedIn(
                        productRecommendationsPreference = action.isProductRecommendationsEnabled,
                    )
                }
            } else {
                ReviewQualityCheckState.NotOptedIn
            }
        }

        ReviewQualityCheckAction.OptOut -> {
            ReviewQualityCheckState.NotOptedIn
        }

        ReviewQualityCheckAction.ToggleProductRecommendation -> {
            if (state is ReviewQualityCheckState.OptedIn && state.productRecommendationsPreference != null) {
                state.copy(productRecommendationsPreference = !state.productRecommendationsPreference)
            } else {
                state
            }
        }

        is ReviewQualityCheckAction.UpdateProductReview -> {
            if (state is ReviewQualityCheckState.OptedIn) {
                state.copy(productReviewState = action.productReviewState)
            } else {
                state
            }
        }
    }
}
