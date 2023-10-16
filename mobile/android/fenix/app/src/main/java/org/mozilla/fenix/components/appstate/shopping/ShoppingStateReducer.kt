/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.appstate.shopping

import org.mozilla.fenix.components.appstate.AppAction.ShoppingAction
import org.mozilla.fenix.components.appstate.AppState

/**
 * Reducer for the shopping state that handles [ShoppingAction]s.
 */
internal object ShoppingStateReducer {

    /**
     * Reduces the given [ShoppingAction] into a new [AppState].
     */
    fun reduce(state: AppState, action: ShoppingAction): AppState =
        when (action) {
            is ShoppingAction.ShoppingSheetStateUpdated -> state.copy(
                shoppingState = state.shoppingState.copy(
                    shoppingSheetExpanded = action.expanded,
                ),
            )

            is ShoppingAction.AddToProductAnalysed -> state.copy(
                shoppingState = state.shoppingState.copy(
                    productsInAnalysis = state.shoppingState.productsInAnalysis + action.productPageUrl,
                ),
            )

            is ShoppingAction.RemoveFromProductAnalysed -> state.copy(
                shoppingState = state.shoppingState.copy(
                    productsInAnalysis = state.shoppingState.productsInAnalysis - action.productPageUrl,
                ),
            )
        }
}
