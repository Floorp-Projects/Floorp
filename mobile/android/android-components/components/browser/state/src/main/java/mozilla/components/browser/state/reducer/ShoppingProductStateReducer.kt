/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.reducer

import mozilla.components.browser.state.action.ShoppingProductAction
import mozilla.components.browser.state.state.BrowserState

internal object ShoppingProductStateReducer {

    fun reduce(state: BrowserState, action: ShoppingProductAction): BrowserState = when (action) {
        is ShoppingProductAction.UpdateProductUrlStatusAction ->
            state.updateTabOrCustomTabState(action.tabId) { current ->
                current.createCopy(isProductUrl = action.isProductUrl)
            }
    }
}
