/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.shopping.store

import junit.framework.TestCase.assertEquals
import kotlinx.coroutines.test.runTest
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.mock
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.whenever
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.shopping.ProductAnalysisTestData
import org.mozilla.fenix.shopping.middleware.NetworkChecker
import org.mozilla.fenix.shopping.middleware.ReviewQualityCheckNetworkMiddleware
import org.mozilla.fenix.shopping.middleware.ReviewQualityCheckPreferences
import org.mozilla.fenix.shopping.middleware.ReviewQualityCheckPreferencesMiddleware
import org.mozilla.fenix.shopping.middleware.ReviewQualityCheckService

class ReviewQualityCheckStoreTest {

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()
    private val dispatcher = coroutinesTestRule.testDispatcher
    private val scope = coroutinesTestRule.scope

    @Test
    fun `GIVEN the user has not opted in the feature WHEN store is created THEN state should display not opted in UI`() =
        runTest {
            val tested = ReviewQualityCheckStore(
                middleware = provideReviewQualityCheckMiddleware(
                    reviewQualityCheckPreferences = FakeReviewQualityCheckPreferences(
                        isEnabled = false,
                        isProductRecommendationsEnabled = false,
                    ),
                ),
            )
            tested.waitUntilIdle()
            dispatcher.scheduler.advanceUntilIdle()
            tested.waitUntilIdle()

            val expected = ReviewQualityCheckState.NotOptedIn
            assertEquals(expected, tested.state)
        }

    @Test
    fun `GIVEN the user has not opted in the feature WHEN the user opts in THEN state should display opted in UI`() =
        runTest {
            var cfrConditionUpdated = false
            val tested = ReviewQualityCheckStore(
                middleware = provideReviewQualityCheckMiddleware(
                    reviewQualityCheckPreferences = FakeReviewQualityCheckPreferences(
                        isEnabled = false,
                        isProductRecommendationsEnabled = false,
                        updateCFRCallback = { cfrConditionUpdated = true },
                    ),
                ),
            )
            tested.waitUntilIdle()
            dispatcher.scheduler.advanceUntilIdle()
            tested.dispatch(ReviewQualityCheckAction.OptIn).joinBlocking()
            tested.waitUntilIdle()
            dispatcher.scheduler.advanceUntilIdle()

            val expected = ReviewQualityCheckState.OptedIn(productRecommendationsPreference = false)
            assertEquals(expected, tested.state)
            assertEquals(true, cfrConditionUpdated)
        }

    @Test
    fun `GIVEN the user has opted in the feature WHEN the user opts out THEN state should display not opted in UI`() =
        runTest {
            val tested = ReviewQualityCheckStore(
                middleware = provideReviewQualityCheckMiddleware(
                    reviewQualityCheckPreferences = FakeReviewQualityCheckPreferences(
                        isEnabled = true,
                        isProductRecommendationsEnabled = true,
                    ),
                ),
            )
            tested.waitUntilIdle()
            dispatcher.scheduler.advanceUntilIdle()
            tested.dispatch(ReviewQualityCheckAction.OptOut).joinBlocking()
            tested.waitUntilIdle()
            dispatcher.scheduler.advanceUntilIdle()

            val expected = ReviewQualityCheckState.NotOptedIn
            assertEquals(expected, tested.state)
        }

    @Test
    fun `GIVEN the user has opted in the feature and product recommendations feature is disabled THEN state should reflect that`() =
        runTest {
            val tested = ReviewQualityCheckStore(
                middleware = provideReviewQualityCheckMiddleware(
                    reviewQualityCheckPreferences = FakeReviewQualityCheckPreferences(
                        isEnabled = true,
                        isProductRecommendationsEnabled = null,
                    ),
                ),
            )
            tested.waitUntilIdle()
            dispatcher.scheduler.advanceUntilIdle()
            tested.waitUntilIdle()

            val expected = ReviewQualityCheckState.OptedIn(productRecommendationsPreference = null)
            assertEquals(expected, tested.state)

            // Even if toggle action is dispatched, state is not changed
            tested.dispatch(ReviewQualityCheckAction.ToggleProductRecommendation).joinBlocking()
            tested.waitUntilIdle()
            assertEquals(expected, tested.state)
        }

    @Test
    fun `GIVEN the user has opted in the feature and product recommendations are off WHEN the user turns on product recommendations THEN state should reflect that`() =
        runTest {
            val tested = ReviewQualityCheckStore(
                middleware = provideReviewQualityCheckMiddleware(
                    reviewQualityCheckPreferences = FakeReviewQualityCheckPreferences(
                        isEnabled = true,
                        isProductRecommendationsEnabled = false,
                    ),
                ),
            )
            tested.waitUntilIdle()
            dispatcher.scheduler.advanceUntilIdle()
            tested.dispatch(ReviewQualityCheckAction.ToggleProductRecommendation).joinBlocking()
            tested.waitUntilIdle()
            dispatcher.scheduler.advanceUntilIdle()

            val expected = ReviewQualityCheckState.OptedIn(productRecommendationsPreference = true)
            assertEquals(expected, tested.state)
        }

    @Test
    fun `GIVEN the user has opted in the feature and product recommendations are on WHEN the user turns off product recommendations THEN state should reflect that`() =
        runTest {
            val tested = ReviewQualityCheckStore(
                middleware = provideReviewQualityCheckMiddleware(
                    reviewQualityCheckPreferences = FakeReviewQualityCheckPreferences(
                        isEnabled = true,
                        isProductRecommendationsEnabled = true,
                    ),
                ),
            )
            tested.waitUntilIdle()
            dispatcher.scheduler.advanceUntilIdle()
            tested.dispatch(ReviewQualityCheckAction.ToggleProductRecommendation).joinBlocking()
            tested.waitUntilIdle()
            dispatcher.scheduler.advanceUntilIdle()

            val expected = ReviewQualityCheckState.OptedIn(productRecommendationsPreference = false)
            assertEquals(expected, tested.state)
        }

    @Test
    fun `GIVEN the user has opted in the feature WHEN the a product analysis is fetched successfully THEN state should reflect that`() =
        runTest {
            val reviewQualityCheckService = mock<ReviewQualityCheckService>()
            whenever(reviewQualityCheckService.fetchProductReview())
                .thenReturn(ProductAnalysisTestData.productAnalysis())

            val tested = ReviewQualityCheckStore(
                middleware = provideReviewQualityCheckMiddleware(
                    reviewQualityCheckPreferences = FakeReviewQualityCheckPreferences(isEnabled = true),
                    reviewQualityCheckService = reviewQualityCheckService,
                    networkChecker = FakeNetworkChecker(isConnected = true),
                ),
            )
            tested.waitUntilIdle()
            dispatcher.scheduler.advanceUntilIdle()
            tested.waitUntilIdle()
            tested.dispatch(ReviewQualityCheckAction.FetchProductAnalysis).joinBlocking()
            tested.waitUntilIdle()
            dispatcher.scheduler.advanceUntilIdle()

            val expected = ReviewQualityCheckState.OptedIn(
                productReviewState = ProductAnalysisTestData.analysisPresent(),
                productRecommendationsPreference = false,
            )
            assertEquals(expected, tested.state)
        }

    @Test
    fun `GIVEN the user has opted in the feature WHEN the a product analysis returns an error THEN state should reflect that`() =
        runTest {
            val reviewQualityCheckService = mock<ReviewQualityCheckService>()
            whenever(reviewQualityCheckService.fetchProductReview()).thenReturn(null)

            val tested = ReviewQualityCheckStore(
                middleware = provideReviewQualityCheckMiddleware(
                    reviewQualityCheckPreferences = FakeReviewQualityCheckPreferences(isEnabled = true),
                    reviewQualityCheckService = reviewQualityCheckService,
                    networkChecker = FakeNetworkChecker(isConnected = true),
                ),
            )
            tested.waitUntilIdle()
            dispatcher.scheduler.advanceUntilIdle()
            tested.waitUntilIdle()
            tested.dispatch(ReviewQualityCheckAction.FetchProductAnalysis).joinBlocking()
            tested.waitUntilIdle()
            dispatcher.scheduler.advanceUntilIdle()

            val expected = ReviewQualityCheckState.OptedIn(
                productReviewState = ReviewQualityCheckState.OptedIn.ProductReviewState.Error.GenericError,
                productRecommendationsPreference = false,
            )
            assertEquals(expected, tested.state)
        }

    @Test
    fun `GIVEN the user has opted in the feature WHEN device is not connected to the internet THEN state should reflect that`() =
        runTest {
            val tested = ReviewQualityCheckStore(
                middleware = provideReviewQualityCheckMiddleware(
                    reviewQualityCheckPreferences = FakeReviewQualityCheckPreferences(isEnabled = true),
                    reviewQualityCheckService = mock(),
                    networkChecker = FakeNetworkChecker(isConnected = false),
                ),
            )
            tested.waitUntilIdle()
            dispatcher.scheduler.advanceUntilIdle()
            tested.waitUntilIdle()
            tested.dispatch(ReviewQualityCheckAction.FetchProductAnalysis).joinBlocking()
            tested.waitUntilIdle()
            dispatcher.scheduler.advanceUntilIdle()

            val expected = ReviewQualityCheckState.OptedIn(
                productReviewState = ReviewQualityCheckState.OptedIn.ProductReviewState.Error.NetworkError,
                productRecommendationsPreference = false,
            )
            assertEquals(expected, tested.state)
        }

    private fun provideReviewQualityCheckMiddleware(
        reviewQualityCheckPreferences: ReviewQualityCheckPreferences,
        reviewQualityCheckService: ReviewQualityCheckService? = null,
        networkChecker: NetworkChecker? = null,
    ): List<ReviewQualityCheckMiddleware> {
        return if (reviewQualityCheckService != null && networkChecker != null) {
            listOf(
                ReviewQualityCheckPreferencesMiddleware(
                    reviewQualityCheckPreferences = reviewQualityCheckPreferences,
                    scope = this.scope,
                ),
                ReviewQualityCheckNetworkMiddleware(
                    reviewQualityCheckService = reviewQualityCheckService,
                    networkChecker = networkChecker,
                    scope = this.scope,
                ),
            )
        } else {
            listOf(
                ReviewQualityCheckPreferencesMiddleware(
                    reviewQualityCheckPreferences = reviewQualityCheckPreferences,
                    scope = this.scope,
                ),
            )
        }
    }
}

private class FakeReviewQualityCheckPreferences(
    private val isEnabled: Boolean = false,
    private val isProductRecommendationsEnabled: Boolean? = false,
    private val updateCFRCallback: () -> Unit = { },
) : ReviewQualityCheckPreferences {
    override suspend fun enabled(): Boolean = isEnabled

    override suspend fun productRecommendationsEnabled(): Boolean? = isProductRecommendationsEnabled

    override suspend fun setEnabled(isEnabled: Boolean) {
    }

    override suspend fun setProductRecommendationsEnabled(isEnabled: Boolean) {
    }

    override suspend fun updateCFRCondition(time: Long) {
        updateCFRCallback()
    }
}

private class FakeNetworkChecker(
    private val isConnected: Boolean,
) : NetworkChecker {
    override fun isConnected(): Boolean = isConnected
}
