/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.appstate

import mozilla.components.support.test.ext.joinBlocking
import org.junit.Assert.assertEquals
import org.junit.Test
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.appstate.shopping.ShoppingState

class ShoppingActionTest {

    @Test
    fun `WHEN shopping sheet is collapsed THEN state should reflect that`() {
        val store = AppStore(
            initialState = AppState(
                shoppingState = ShoppingState(
                    shoppingSheetExpanded = true,
                ),
            ),
        )

        store.dispatch(AppAction.ShoppingAction.ShoppingSheetStateUpdated(false)).joinBlocking()

        val expected = ShoppingState(
            shoppingSheetExpanded = false,
        )

        assertEquals(expected, store.state.shoppingState)
    }

    @Test
    fun `WHEN shopping sheet is expanded THEN state should reflect that`() {
        val store = AppStore()

        store.dispatch(AppAction.ShoppingAction.ShoppingSheetStateUpdated(true)).joinBlocking()

        val expected = ShoppingState(
            shoppingSheetExpanded = true,
        )

        assertEquals(expected, store.state.shoppingState)
    }

    @Test
    fun `WHEN product analysed is added THEN state should reflect that`() {
        val store = AppStore()

        store.dispatch(AppAction.ShoppingAction.AddToProductAnalysed("pdp")).joinBlocking()

        val expected = ShoppingState(
            productsInAnalysis = setOf("pdp"),
        )

        assertEquals(expected, store.state.shoppingState)
    }

    @Test
    fun `WHEN product analysed is removed THEN state should reflect that`() {
        val store = AppStore(
            initialState = AppState(
                shoppingState = ShoppingState(
                    productsInAnalysis = setOf("pdp"),
                ),
            ),
        )

        store.dispatch(AppAction.ShoppingAction.RemoveFromProductAnalysed("pdp")).joinBlocking()

        val expected = ShoppingState(
            productsInAnalysis = emptySet(),
        )

        assertEquals(expected, store.state.shoppingState)
    }
}
