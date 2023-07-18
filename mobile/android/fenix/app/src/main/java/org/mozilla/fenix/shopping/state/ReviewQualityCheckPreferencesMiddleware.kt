/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.shopping.state

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.launch
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext
import mozilla.components.lib.state.Store

/**
 * Middleware for getting and setting review quality check user preferences.
 *
 * @param reviewQualityCheckPreferences The [ReviewQualityCheckPreferences] instance to use.
 * @param scope The [CoroutineScope] to use for launching coroutines.
 */
class ReviewQualityCheckPreferencesMiddleware(
    private val reviewQualityCheckPreferences: ReviewQualityCheckPreferences,
    private val scope: CoroutineScope,
) : Middleware<ReviewQualityCheckState, ReviewQualityCheckAction> {

    override fun invoke(
        context: MiddlewareContext<ReviewQualityCheckState, ReviewQualityCheckAction>,
        next: (ReviewQualityCheckAction) -> Unit,
        action: ReviewQualityCheckAction,
    ) {
        when (action) {
            is ReviewQualityCheckAction.PreferencesMiddlewareAction -> {
                processAction(context.store, action)
            }

            else -> {
                // no-op
            }
        }
        // Forward the actions
        next(action)
    }

    private fun processAction(
        store: Store<ReviewQualityCheckState, ReviewQualityCheckAction>,
        action: ReviewQualityCheckAction.PreferencesMiddlewareAction,
    ) {
        when (action) {
            is ReviewQualityCheckAction.Init -> {
                scope.launch {
                    val hasUserOptedIn = reviewQualityCheckPreferences.enabled()
                    val isProductRecommendationsEnabled =
                        reviewQualityCheckPreferences.productRecommendationsEnabled()
                    store.dispatch(
                        ReviewQualityCheckAction.UpdateUserPreferences(
                            hasUserOptedIn = hasUserOptedIn,
                            isProductRecommendationsEnabled = isProductRecommendationsEnabled,
                        ),
                    )
                }
            }

            ReviewQualityCheckAction.OptIn -> {
                scope.launch {
                    val isProductRecommendationsEnabled =
                        reviewQualityCheckPreferences.productRecommendationsEnabled()
                    store.dispatch(
                        ReviewQualityCheckAction.UpdateUserPreferences(
                            hasUserOptedIn = true,
                            isProductRecommendationsEnabled = isProductRecommendationsEnabled,
                        ),
                    )

                    // Update the preference
                    reviewQualityCheckPreferences.setEnabled(true)
                }
            }

            ReviewQualityCheckAction.OptOut -> {
                scope.launch {
                    // Update the preference
                    reviewQualityCheckPreferences.setEnabled(false)
                }
            }

            ReviewQualityCheckAction.ToggleProductRecommendation -> {
                scope.launch {
                    reviewQualityCheckPreferences.setProductRecommendationsEnabled(
                        !reviewQualityCheckPreferences.productRecommendationsEnabled(),
                    )
                }
            }
        }
    }
}
