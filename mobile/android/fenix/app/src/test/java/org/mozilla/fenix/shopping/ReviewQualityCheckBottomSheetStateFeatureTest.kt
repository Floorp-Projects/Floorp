/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.shopping

import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.shopping.store.ReviewQualityCheckAction
import org.mozilla.fenix.shopping.store.ReviewQualityCheckState
import org.mozilla.fenix.shopping.store.ReviewQualityCheckStore

class ReviewQualityCheckBottomSheetStateFeatureTest {

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    @Test
    fun `WHEN store state changes to not opted in from any other state THEN callback is not invoked`() {
        val store = ReviewQualityCheckStore(emptyList())
        var isInvoked = false
        val tested = ReviewQualityCheckBottomSheetStateFeature(
            store = store,
            onRequestStateExpanded = {
                isInvoked = true
            },
        )

        tested.start()
        store.dispatch(
            ReviewQualityCheckAction.OptInCompleted(
                isProductRecommendationsEnabled = true,
                productVendor = ReviewQualityCheckState.ProductVendor.WALMART,
            ),
        ).joinBlocking()
        store.dispatch(ReviewQualityCheckAction.OptOutCompleted(emptyList())).joinBlocking()

        assertFalse(isInvoked)
    }

    @Test
    fun `WHEN store state changes to not opted in from initial state THEN callback is invoked`() {
        val store = ReviewQualityCheckStore(emptyList())
        var isInvoked = false
        val tested = ReviewQualityCheckBottomSheetStateFeature(
            store = store,
            onRequestStateExpanded = {
                isInvoked = true
            },
        )
        assertEquals(ReviewQualityCheckState.Initial, store.state)

        tested.start()
        store.dispatch(ReviewQualityCheckAction.OptOutCompleted(emptyList())).joinBlocking()

        assertTrue(isInvoked)
    }
}
