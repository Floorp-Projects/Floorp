/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.shopping.middleware

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.launch
import mozilla.components.browser.engine.gecko.shopping.GeckoProductAnalysis
import mozilla.components.lib.state.MiddlewareContext
import mozilla.components.lib.state.Store
import mozilla.components.support.base.log.logger.Logger
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.appstate.AppAction.ShoppingAction
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
 * @property appStore The [AppStore] instance to access state and dispatch [ShoppingAction]s.
 * @property scope The [CoroutineScope] that will be used to launch coroutines.
 */
class ReviewQualityCheckNetworkMiddleware(
    private val reviewQualityCheckService: ReviewQualityCheckService,
    private val networkChecker: NetworkChecker,
    private val appStore: AppStore,
    private val scope: CoroutineScope,
) : ReviewQualityCheckMiddleware {

    private val logger = Logger("ReviewQualityCheckNetworkMiddleware")

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
                        store.updateProductReviewState(status.toProductReviewState())
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
            block = {
                logger.debug("Retrying")
                reviewQualityCheckService.analysisStatus()
            },
        )

    private suspend fun AnalysisStatusDto.toProductReviewState(): ProductReviewState =
        when (this) {
            AnalysisStatusDto.COMPLETED ->
                reviewQualityCheckService.fetchProductReview().toProductReviewState()

            AnalysisStatusDto.NOT_ANALYZABLE -> ProductReviewState.Error.UnsupportedProductTypeError
            else -> ProductReviewState.Error.GenericError
        }

    private fun Store<ReviewQualityCheckState, ReviewQualityCheckAction>.updateProductReviewState(
        productReviewState: ProductReviewState,
    ) {
        dispatch(ReviewQualityCheckAction.UpdateProductReview(productReviewState))
    }

    private fun Store<ReviewQualityCheckState, ReviewQualityCheckAction>.restoreAnalysingStateIfRequired(
        productPageUrl: String,
        productReviewState: ProductReviewState,
        productAnalysis: GeckoProductAnalysis?,
    ) {
        if (productReviewState.isAnalysisPresentOrNoAnalysisPresent() &&
            productAnalysis?.needsAnalysis == true &&
            appStore.state.shoppingState.productsInAnalysis.contains(productPageUrl)
        ) {
            logger.debug("Found product in the set: $productPageUrl")
            dispatch(ReviewQualityCheckAction.ReanalyzeProduct)
        }
    }

    private fun ProductReviewState.isAnalysisPresentOrNoAnalysisPresent() =
        this is ProductReviewState.AnalysisPresent || this is ProductReviewState.NoAnalysisPresent
}
