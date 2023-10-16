/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.shopping

import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Assert.assertEquals
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.shopping.store.BottomSheetViewState
import org.mozilla.fenix.shopping.store.ReviewQualityCheckAction
import org.mozilla.fenix.shopping.store.ReviewQualityCheckState
import org.mozilla.fenix.shopping.store.ReviewQualityCheckStore

class ReviewQualityCheckBottomSheetStateFeatureTest {

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    @Test
    fun `WHEN store state changes to not opted in from any other state THEN callback is invoked with half state`() {
        val store = ReviewQualityCheckStore(emptyList())
        var updatedState: BottomSheetViewState? = null
        val tested = ReviewQualityCheckBottomSheetStateFeature(
            store = store,
            onRequestStateUpdate = {
                updatedState = it
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

        assertEquals(BottomSheetViewState.HALF_VIEW, updatedState)
    }

    @Test
    fun `WHEN store state changes to not opted in from initial state THEN callback is invoked with full state`() {
        val store = ReviewQualityCheckStore(emptyList())
        var updatedState: BottomSheetViewState? = null
        val tested = ReviewQualityCheckBottomSheetStateFeature(
            store = store,
            onRequestStateUpdate = {
                updatedState = it
            },
        )
        assertEquals(ReviewQualityCheckState.Initial, store.state)

        tested.start()
        store.dispatch(ReviewQualityCheckAction.OptOutCompleted(emptyList())).joinBlocking()

        assertEquals(BottomSheetViewState.FULL_VIEW, updatedState)
    }
}
