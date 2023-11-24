/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.appstate

import mozilla.components.support.test.ext.joinBlocking
import org.junit.Assert.assertEquals
import org.junit.Test
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.appstate.AppAction.ShoppingAction.HighlightsCardExpanded
import org.mozilla.fenix.components.appstate.AppAction.ShoppingAction.InfoCardExpanded
import org.mozilla.fenix.components.appstate.AppAction.ShoppingAction.SettingsCardExpanded
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
    fun `WHEN product analysis highlights card is expanded THEN state should reflect that`() {
        val store = AppStore(initialState = AppState(shoppingState = ShoppingState()))

        store.dispatch(HighlightsCardExpanded(productPageUrl = "pdp", expanded = true))
            .joinBlocking()

        val expected = ShoppingState(
            productCardState = mapOf(
                "pdp" to ShoppingState.CardState(isHighlightsExpanded = true),
            ),
        )

        assertEquals(expected, store.state.shoppingState)
    }

    @Test
    fun `WHEN product analysis highlights card is collapsed THEN state should reflect that`() {
        val store = AppStore(initialState = AppState(shoppingState = ShoppingState()))

        store.dispatch(HighlightsCardExpanded(productPageUrl = "pdp", expanded = false))
            .joinBlocking()

        val expected = ShoppingState(
            productCardState = mapOf(
                "pdp" to ShoppingState.CardState(isHighlightsExpanded = false),
            ),
        )

        assertEquals(expected, store.state.shoppingState)
    }

    @Test
    fun `WHEN product analysis info card is expanded THEN state should reflect that`() {
        val store = AppStore(
            initialState = AppState(
                shoppingState = ShoppingState(
                    productCardState = mapOf(
                        "1" to ShoppingState.CardState(
                            isHighlightsExpanded = true,
                            isSettingsExpanded = false,
                        ),
                    ),
                ),
            ),
        )

        store.dispatch(InfoCardExpanded(productPageUrl = "2", expanded = true))
            .joinBlocking()

        val expected = ShoppingState(
            productCardState = mapOf(
                "1" to ShoppingState.CardState(
                    isHighlightsExpanded = true,
                    isSettingsExpanded = false,
                ),
                "2" to ShoppingState.CardState(
                    isInfoExpanded = true,
                ),
            ),
        )

        assertEquals(expected, store.state.shoppingState)
    }

    @Test
    fun `WHEN product analysis info card is collapsed THEN state should reflect that`() {
        val store = AppStore(
            initialState = AppState(
                shoppingState = ShoppingState(
                    productCardState = mapOf(
                        "1" to ShoppingState.CardState(
                            isHighlightsExpanded = true,
                            isSettingsExpanded = false,
                        ),
                        "2" to ShoppingState.CardState(
                            isInfoExpanded = true,
                        ),
                    ),
                ),
            ),
        )

        store.dispatch(InfoCardExpanded(productPageUrl = "2", expanded = false))
            .joinBlocking()

        val expected = ShoppingState(
            productCardState = mapOf(
                "1" to ShoppingState.CardState(
                    isHighlightsExpanded = true,
                    isSettingsExpanded = false,
                ),
                "2" to ShoppingState.CardState(
                    isInfoExpanded = false,
                ),
            ),
        )

        assertEquals(expected, store.state.shoppingState)
    }

    @Test
    fun `WHEN product analysis settings card is expanded THEN state should reflect that`() {
        val store = AppStore(
            initialState = AppState(
                shoppingState = ShoppingState(
                    productCardState = mapOf(
                        "pdp" to ShoppingState.CardState(
                            isHighlightsExpanded = true,
                            isSettingsExpanded = false,
                        ),
                    ),
                ),
            ),
        )

        store.dispatch(SettingsCardExpanded(productPageUrl = "pdp", expanded = true))
            .joinBlocking()

        val expected = ShoppingState(
            productCardState = mapOf(
                "pdp" to ShoppingState.CardState(
                    isHighlightsExpanded = true,
                    isSettingsExpanded = true,
                ),
            ),
        )

        assertEquals(expected, store.state.shoppingState)
    }

    @Test
    fun `WHEN product analysis settings card is collapsed THEN state should reflect that`() {
        val store = AppStore(
            initialState = AppState(
                shoppingState = ShoppingState(
                    productCardState = mapOf(
                        "pdp" to ShoppingState.CardState(
                            isSettingsExpanded = true,
                        ),
                    ),
                ),
            ),
        )

        store.dispatch(SettingsCardExpanded(productPageUrl = "pdp", expanded = false))
            .joinBlocking()

        val expected = ShoppingState(
            productCardState = mapOf(
                "pdp" to ShoppingState.CardState(isSettingsExpanded = false),
            ),
        )

        assertEquals(expected, store.state.shoppingState)
    }
}
