/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.shopping.middleware

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.launch
import mozilla.components.concept.engine.shopping.ProductAnalysis
import mozilla.components.lib.state.MiddlewareContext
import mozilla.components.lib.state.Store
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.appstate.AppAction.ShoppingAction
import org.mozilla.fenix.shopping.store.ReviewQualityCheckAction
import org.mozilla.fenix.shopping.store.ReviewQualityCheckAction.FetchProductAnalysis
import org.mozilla.fenix.shopping.store.ReviewQualityCheckAction.RetryProductAnalysis
import org.mozilla.fenix.shopping.store.ReviewQualityCheckAction.UpdateRecommendedProduct
import org.mozilla.fenix.shopping.store.ReviewQualityCheckMiddleware
import org.mozilla.fenix.shopping.store.ReviewQualityCheckState
import org.mozilla.fenix.shopping.store.ReviewQualityCheckState.OptedIn.ProductReviewState
import org.mozilla.fenix.shopping.store.ReviewQualityCheckState.OptedIn.ProductReviewState.AnalysisPresent.AnalysisStatus
import org.mozilla.fenix.shopping.store.ReviewQualityCheckState.RecommendedProductState

/**
 * Middleware that handles network requests for the review quality check feature.
 *
 * @property reviewQualityCheckService The service that handles the network requests.
 * @property networkChecker The [NetworkChecker] instance to check the network status.
 * @property appStore The [AppStore] instance to access state and dispatch [ShoppingAction]s.
 * @property scope The [CoroutineScope] that will be used to launch coroutines.
 */
class ReviewQualityCheckNetworkMiddleware(
    private val reviewQualityCheckService: ReviewQualityCheckService,
    private val networkChecker: NetworkChecker,
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
                    val productPageUrl = reviewQualityCheckService.selectedTabUrl()
                    val productAnalysis = reviewQualityCheckService.fetchProductReview()
                    val productReviewState = productAnalysis.toProductReviewState()
                    store.updateProductReviewState(productReviewState)

                    productPageUrl?.let {
                        store.restoreAnalysingStateIfRequired(
                            productPageUrl = productPageUrl,
                            productReviewState = productReviewState,
                            productAnalysis = productAnalysis,
                        )
                    }

                    if (productReviewState is ProductReviewState.AnalysisPresent) {
                        store.updateRecommendedProductState()
                    }
                }

                ReviewQualityCheckAction.ReanalyzeProduct, ReviewQualityCheckAction.AnalyzeProduct -> {
                    val reanalysis = reviewQualityCheckService.reanalyzeProduct()

                    if (reanalysis == null) {
                        store.updateProductReviewState(ProductReviewState.Error.GenericError)
                        return@launch
                    }

                    // add product to the set of products that are being analysed
                    val productPageUrl = reviewQualityCheckService.selectedTabUrl()
                    productPageUrl?.let {
                        appStore.dispatch(ShoppingAction.AddToProductAnalysed(it))
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
                        val productAnalysis = reviewQualityCheckService.fetchProductReview()
                        val productReviewState = productAnalysis.toProductReviewState(false)
                        store.updateProductReviewState(productReviewState)
                    }

                    // remove product from the set of products that are being analysed
                    productPageUrl?.let {
                        appStore.dispatch(ShoppingAction.RemoveFromProductAnalysed(it))
                    }
                }
            }
        }
    }

    private suspend fun pollForAnalysisStatus(): AnalysisStatusDto? =
        retry(
            predicate = { it == AnalysisStatusDto.PENDING || it == AnalysisStatusDto.IN_PROGRESS },
            block = { reviewQualityCheckService.analysisStatus() },
        )

    private fun Store<ReviewQualityCheckState, ReviewQualityCheckAction>.updateProductReviewState(
        productReviewState: ProductReviewState,
    ) {
        dispatch(ReviewQualityCheckAction.UpdateProductReview(productReviewState))
    }

    private fun Store<ReviewQualityCheckState, ReviewQualityCheckAction>.restoreAnalysingStateIfRequired(
        productPageUrl: String,
        productReviewState: ProductReviewState,
        productAnalysis: ProductAnalysis?,
    ) {
        if (productReviewState.isAnalysisPresentOrNoAnalysisPresent() &&
            productAnalysis?.needsAnalysis == true &&
            appStore.state.shoppingState.productsInAnalysis.contains(productPageUrl)
        ) {
            dispatch(ReviewQualityCheckAction.ReanalyzeProduct)
        }
    }

    private fun ProductReviewState.isAnalysisPresentOrNoAnalysisPresent() =
        this is ProductReviewState.AnalysisPresent || this is ProductReviewState.NoAnalysisPresent

    private suspend fun Store<ReviewQualityCheckState, ReviewQualityCheckAction>.updateRecommendedProductState() {
        val currentState = state
        if (currentState is ReviewQualityCheckState.OptedIn &&
            currentState.productRecommendationsPreference == true
        ) {
            dispatch(UpdateRecommendedProduct(RecommendedProductState.Loading))
            reviewQualityCheckService.productRecommendation().toRecommendedProductState().also {
                dispatch(UpdateRecommendedProduct(it))
            }
        }
    }
}
