/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.shopping.store

import junit.framework.TestCase.assertEquals
import kotlinx.coroutines.test.runTest
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.shopping.middleware.ReviewQualityCheckPreferences
import org.mozilla.fenix.shopping.middleware.ReviewQualityCheckPreferencesMiddleware

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
            dispatcher.scheduler.advanceUntilIdle()
            tested.waitUntilIdle()

            val expected = ReviewQualityCheckState.NotOptedIn
            assertEquals(expected, tested.state)
        }

    @Test
    fun `GIVEN the user has not opted in the feature WHEN the user opts in THEN state should display opted in UI`() =
        runTest {
            val tested = ReviewQualityCheckStore(
                middleware = provideReviewQualityCheckMiddleware(
                    reviewQualityCheckPreferences = FakeReviewQualityCheckPreferences(
                        isEnabled = false,
                        isProductRecommendationsEnabled = false,
                    ),
                ),
            )
            dispatcher.scheduler.advanceUntilIdle()
            tested.waitUntilIdle()
            tested.dispatch(ReviewQualityCheckAction.OptIn).joinBlocking()
            dispatcher.scheduler.advanceUntilIdle()
            tested.waitUntilIdle()

            val expected = ReviewQualityCheckState.OptedIn(productRecommendationsPreference = false)
            assertEquals(expected, tested.state)
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
            dispatcher.scheduler.advanceUntilIdle()
            tested.waitUntilIdle()
            tested.dispatch(ReviewQualityCheckAction.OptOut).joinBlocking()
            dispatcher.scheduler.advanceUntilIdle()
            tested.waitUntilIdle()

            val expected = ReviewQualityCheckState.NotOptedIn
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
            dispatcher.scheduler.advanceUntilIdle()
            tested.waitUntilIdle()
            tested.dispatch(ReviewQualityCheckAction.ToggleProductRecommendation).joinBlocking()
            dispatcher.scheduler.advanceUntilIdle()
            tested.waitUntilIdle()

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
            dispatcher.scheduler.advanceUntilIdle()
            tested.waitUntilIdle()
            tested.dispatch(ReviewQualityCheckAction.ToggleProductRecommendation).joinBlocking()
            dispatcher.scheduler.advanceUntilIdle()
            tested.waitUntilIdle()

            val expected = ReviewQualityCheckState.OptedIn(productRecommendationsPreference = false)
            assertEquals(expected, tested.state)
        }

    private fun provideReviewQualityCheckMiddleware(
        reviewQualityCheckPreferences: ReviewQualityCheckPreferences,
    ): List<ReviewQualityCheckMiddleware> =
        listOf(
            ReviewQualityCheckPreferencesMiddleware(
                reviewQualityCheckPreferences = reviewQualityCheckPreferences,
                scope = this.scope,
            ),
        )
}

private class FakeReviewQualityCheckPreferences(
    private val isEnabled: Boolean = false,
    private val isProductRecommendationsEnabled: Boolean = false,
) : ReviewQualityCheckPreferences {
    override suspend fun enabled(): Boolean = isEnabled

    override suspend fun productRecommendationsEnabled(): Boolean = isProductRecommendationsEnabled

    override suspend fun setEnabled(isEnabled: Boolean) {
    }

    override suspend fun setProductRecommendationsEnabled(isEnabled: Boolean) {
    }
}
