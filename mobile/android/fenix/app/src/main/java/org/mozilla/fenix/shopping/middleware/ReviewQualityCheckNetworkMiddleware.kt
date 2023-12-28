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
import org.mozilla.fenix.shopping.store.ReviewQualityCheckAction.UpdateRecommendedProduct
import org.mozilla.fenix.shopping.store.ReviewQualityCheckMiddleware
import org.mozilla.fenix.shopping.store.ReviewQualityCheckState
import org.mozilla.fenix.shopping.store.ReviewQualityCheckState.OptedIn.ProductReviewState
import org.mozilla.fenix.shopping.store.ReviewQualityCheckState.OptedIn.ProductReviewState.AnalysisPresent.AnalysisStatus

/**
 * Middleware that handles network requests for the review quality check feature.
 *
 * @param reviewQualityCheckService The service that handles the network requests.
 * @param networkChecker The [NetworkChecker] instance to check the network status.
 * @param scope The [CoroutineScope] that will be used to launch coroutines.
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
                    store.onFetch()
                }

                ReviewQualityCheckAction.ReanalyzeProduct,
                ReviewQualityCheckAction.AnalyzeProduct,
                ReviewQualityCheckAction.RestoreReanalysis,
                -> {
                    store.onReanalyze()
                }

                ReviewQualityCheckAction.ReportProductBackInStock -> {
                    val status = reviewQualityCheckService.reportBackInStock()
                    if (status == ReportBackInStockStatusDto.NOT_DELETED) {
                        store.onFetch()
                    }
                }

                ReviewQualityCheckAction.ToggleProductRecommendation -> {
                    val state = store.state
                    if (state is ReviewQualityCheckState.OptedIn &&
                        state.productReviewState is ProductReviewState.AnalysisPresent &&
                        state.productRecommendationsPreference == true
                    ) {
                        store.updateRecommendedProductState()
                    }
                }
            }
        }
    }

    private suspend fun Store<ReviewQualityCheckState, ReviewQualityCheckAction>.onFetch() {
        val productAnalysis = reviewQualityCheckService.fetchProductReview()
        val productReviewState = productAnalysis.toProductReviewState()

        // Here the ProductReviewState should only updated after the analysis status API
        // returns a result. This makes sure that the UI doesn't show the reanalyse
        // button in case the product analysis is already in progress on the backend.
        if (productReviewState.isAnalysisPresentOrNoAnalysisPresent() &&
            reviewQualityCheckService.analysisStatus()?.status.isPendingOrInProgress()
        ) {
            updateProductReviewState(productReviewState, true)
            dispatch(ReviewQualityCheckAction.RestoreReanalysis)
        } else {
            updateProductReviewState(productReviewState)
        }

        if (productReviewState is ProductReviewState.AnalysisPresent) {
            updateRecommendedProductState()
        }
    }

    private suspend fun Store<ReviewQualityCheckState, ReviewQualityCheckAction>.onReanalyze() {
        val reanalysis = reviewQualityCheckService.reanalyzeProduct()

        if (reanalysis == null) {
            updateProductReviewState(ProductReviewState.Error.GenericError)
            return
        }

        val statusProgress = pollForAnalysisStatus {
            dispatch(ReviewQualityCheckAction.UpdateAnalysisProgress(it))
        }

        if (statusProgress == null ||
            statusProgress.status == AnalysisStatusDto.PENDING ||
            statusProgress.status == AnalysisStatusDto.IN_PROGRESS
        ) {
            // poll failed, reset to previous state
            val state = this.state
            if (state is ReviewQualityCheckState.OptedIn) {
                if (state.productReviewState is ProductReviewState.NoAnalysisPresent) {
                    updateProductReviewState(ProductReviewState.NoAnalysisPresent())
                } else if (state.productReviewState is ProductReviewState.AnalysisPresent) {
                    updateProductReviewState(
                        state.productReviewState.copy(analysisStatus = AnalysisStatus.NeedsAnalysis),
                    )
                }
            }
        } else {
            // poll succeeded, update state
            val productAnalysis = reviewQualityCheckService.fetchProductReview()
            val productReviewState = productAnalysis.toProductReviewState()
            updateProductReviewState(productReviewState)
        }
    }

    private suspend fun pollForAnalysisStatus(
        onEachSuccessfulPoll: (progress: Double) -> Unit,
    ): AnalysisStatusProgressDto? =
        retry(
            predicate = { it?.status.isPendingOrInProgress() },
            block = {
                reviewQualityCheckService.analysisStatus()?.also {
                    onEachSuccessfulPoll(it.progress)
                }
            },
        )

    private fun Store<ReviewQualityCheckState, ReviewQualityCheckAction>.updateProductReviewState(
        productReviewState: ProductReviewState,
        restoreAnalysis: Boolean = false,
    ) {
        dispatch(ReviewQualityCheckAction.UpdateProductReview(productReviewState, restoreAnalysis))
    }

    private fun ProductReviewState.isAnalysisPresentOrNoAnalysisPresent() =
        this is ProductReviewState.AnalysisPresent || this is ProductReviewState.NoAnalysisPresent

    private suspend fun Store<ReviewQualityCheckState, ReviewQualityCheckAction>.updateRecommendedProductState() {
        val currentState = state
        if (currentState is ReviewQualityCheckState.OptedIn &&
            (currentState.productRecommendationsExposure || (currentState.productRecommendationsPreference == true))
        ) {
            val productRecommendation = reviewQualityCheckService.productRecommendation(
                currentState.productRecommendationsPreference ?: false,
            )
            if (currentState.productRecommendationsPreference == true) {
                productRecommendation.toRecommendedProductState().also {
                    dispatch(UpdateRecommendedProduct(it))
                }
            }
        }
    }

    private fun AnalysisStatusDto?.isPendingOrInProgress(): Boolean =
        this == AnalysisStatusDto.PENDING || this == AnalysisStatusDto.IN_PROGRESS
}
