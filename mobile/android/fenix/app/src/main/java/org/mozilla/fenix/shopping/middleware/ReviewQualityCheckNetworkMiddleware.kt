/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.shopping.middleware

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.launch
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext
import mozilla.components.lib.state.Store
import org.mozilla.fenix.shopping.store.ReviewQualityCheckAction
import org.mozilla.fenix.shopping.store.ReviewQualityCheckAction.FetchProductAnalysis
import org.mozilla.fenix.shopping.store.ReviewQualityCheckAction.RetryProductAnalysis
import org.mozilla.fenix.shopping.store.ReviewQualityCheckState
import org.mozilla.fenix.shopping.store.ReviewQualityCheckState.OptedIn.ProductReviewState

/**
 * Middleware that handles network requests for the review quality check feature.
 *
 * @property reviewQualityCheckService The service that handles the network requests.
 * @property networkChecker The [NetworkChecker] instance to check the network status.
 * @property scope The [CoroutineScope] that will be used to launch coroutines.
 */
class ReviewQualityCheckNetworkMiddleware(
    private val reviewQualityCheckService: ReviewQualityCheckService,
    private val networkChecker: NetworkChecker,
    private val scope: CoroutineScope,
) : Middleware<ReviewQualityCheckState, ReviewQualityCheckAction> {

    override fun invoke(
        context: MiddlewareContext<ReviewQualityCheckState, ReviewQualityCheckAction>,
        next: (ReviewQualityCheckAction) -> Unit,
        action: ReviewQualityCheckAction,
    ) {
        next(action)
        when (action) {
            is ReviewQualityCheckAction.NetworkAction -> processAction(context.store, action)
            else -> {
                // no-op
            }
        }
    }

    private fun processAction(
        store: Store<ReviewQualityCheckState, ReviewQualityCheckAction>,
        action: ReviewQualityCheckAction.NetworkAction,
    ) {
        when (action) {
            FetchProductAnalysis, RetryProductAnalysis -> {
                scope.launch {
                    val productReviewState = if (networkChecker.isConnected()) {
                        reviewQualityCheckService.fetchProductReview().toProductReviewState()
                    } else {
                        ProductReviewState.Error.NetworkError
                    }
                    store.dispatch(ReviewQualityCheckAction.UpdateProductReview(productReviewState))
                }
            }

            ReviewQualityCheckAction.ReanalyzeProduct -> {
                // Bug 1853311 - Integrate analyze and analysis_status
            }
        }
    }
}
