/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.shopping.middleware

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.launch
import mozilla.components.lib.state.MiddlewareContext
import mozilla.components.lib.state.Store
import org.mozilla.fenix.shopping.store.ReviewQualityCheckAction
import org.mozilla.fenix.shopping.store.ReviewQualityCheckMiddleware
import org.mozilla.fenix.shopping.store.ReviewQualityCheckState

/**
 * Middleware for getting and setting review quality check user preferences.
 *
 * @param reviewQualityCheckPreferences The [ReviewQualityCheckPreferences] instance to use.
 * @param scope The [CoroutineScope] to use for launching coroutines.
 */
class ReviewQualityCheckPreferencesMiddleware(
    private val reviewQualityCheckPreferences: ReviewQualityCheckPreferences,
    private val reviewQualityCheckVendorsService: ReviewQualityCheckVendorsService,
    private val scope: CoroutineScope,
) : ReviewQualityCheckMiddleware {

    override fun invoke(
        context: MiddlewareContext<ReviewQualityCheckState, ReviewQualityCheckAction>,
        next: (ReviewQualityCheckAction) -> Unit,
        action: ReviewQualityCheckAction,
    ) {
        next(action)
        when (action) {
            is ReviewQualityCheckAction.PreferencesMiddlewareAction -> {
                processAction(context.store, action)
            }

            else -> {
                // no-op
            }
        }
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

                    val updateUserPreferences = if (hasUserOptedIn) {
                        ReviewQualityCheckAction.OptInCompleted(
                            isProductRecommendationsEnabled = isProductRecommendationsEnabled,
                            productVendor = reviewQualityCheckVendorsService.productVendor(),
                        )
                    } else {
                        val productVendors = reviewQualityCheckVendorsService.productVendors()
                        ReviewQualityCheckAction.OptOutCompleted(productVendors)
                    }
                    store.dispatch(updateUserPreferences)
                }
            }

            ReviewQualityCheckAction.OptIn -> {
                scope.launch {
                    val isProductRecommendationsEnabled =
                        reviewQualityCheckPreferences.productRecommendationsEnabled()
                    store.dispatch(
                        ReviewQualityCheckAction.OptInCompleted(
                            isProductRecommendationsEnabled = isProductRecommendationsEnabled,
                            productVendor = reviewQualityCheckVendorsService.productVendor(),
                        ),
                    )

                    // Update the preference
                    reviewQualityCheckPreferences.setEnabled(true)
                    reviewQualityCheckPreferences.updateCFRCondition(System.currentTimeMillis())
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
                    val productRecommendationsEnabled =
                        reviewQualityCheckPreferences.productRecommendationsEnabled()
                    if (productRecommendationsEnabled != null) {
                        reviewQualityCheckPreferences.setProductRecommendationsEnabled(
                            !productRecommendationsEnabled,
                        )
                    }
                }
            }
        }
    }
}
