/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.shopping.middleware

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.launch
import mozilla.components.lib.state.MiddlewareContext
import mozilla.components.lib.state.Store
import org.mozilla.fenix.shopping.store.ReviewQualityCheckAction
import org.mozilla.fenix.shopping.store.ReviewQualityCheckAction.FetchProductAnalysis
import org.mozilla.fenix.shopping.store.ReviewQualityCheckAction.RetryProductAnalysis
import org.mozilla.fenix.shopping.store.ReviewQualityCheckMiddleware
import org.mozilla.fenix.shopping.store.ReviewQualityCheckState
import org.mozilla.fenix.shopping.store.ReviewQualityCheckState.OptedIn.ProductReviewState
import org.mozilla.fenix.shopping.store.ReviewQualityCheckState.OptedIn.ProductReviewState.AnalysisPresent.AnalysisStatus

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
) : ReviewQualityCheckMiddleware {

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
        if (!networkChecker.isConnected()) {
            store.updateProductReviewState(ProductReviewState.Error.NetworkError)
            return
        }

        scope.launch {
            when (action) {
                FetchProductAnalysis, RetryProductAnalysis -> {
                    val productReviewState = fetchAnalysis()
                    store.updateProductReviewState(productReviewState)
                }

                ReviewQualityCheckAction.ReanalyzeProduct -> {
                    val reanalysis = reviewQualityCheckService.reanalyzeProduct()

                    if (reanalysis == null) {
                        store.updateProductReviewState(ProductReviewState.Error.GenericError)
                        return@launch
                    }

                    val status = pollForAnalysisStatus()

                    if (status == null ||
                        status == AnalysisStatusDto.PENDING ||
                        status == AnalysisStatusDto.IN_PROGRESS
                    ) {
                        // poll failed, reset to previous state
                        val state = store.state
                        if (state is ReviewQualityCheckState.OptedIn) {
                            if (state.productReviewState is ProductReviewState.NoAnalysisPresent) {
                                store.updateProductReviewState(ProductReviewState.NoAnalysisPresent())
                            } else if (state.productReviewState is ProductReviewState.AnalysisPresent) {
                                store.updateProductReviewState(
                                    state.productReviewState.copy(
                                        analysisStatus = AnalysisStatus.NEEDS_ANALYSIS,
                                    ),
                                )
                            }
                        }
                    } else {
                        // poll succeeded, update state
                        store.updateProductReviewState(status.toProductReviewState())
                    }
                }
            }
        }
    }

    private suspend fun fetchAnalysis(): ProductReviewState =
        reviewQualityCheckService.fetchProductReview().toProductReviewState()

    private suspend fun pollForAnalysisStatus(): AnalysisStatusDto? =
        retry(
            predicate = { it == AnalysisStatusDto.PENDING || it == AnalysisStatusDto.IN_PROGRESS },
            block = { reviewQualityCheckService.analysisStatus() },
        )

    private suspend fun AnalysisStatusDto.toProductReviewState(): ProductReviewState =
        when (this) {
            AnalysisStatusDto.COMPLETED -> fetchAnalysis()
            AnalysisStatusDto.NOT_ANALYZABLE -> ProductReviewState.Error.UnsupportedProductTypeError
            else -> ProductReviewState.Error.GenericError
        }

    private fun Store<ReviewQualityCheckState, ReviewQualityCheckAction>.updateProductReviewState(
        productReviewState: ProductReviewState,
    ) {
        dispatch(ReviewQualityCheckAction.UpdateProductReview(productReviewState))
    }
}
