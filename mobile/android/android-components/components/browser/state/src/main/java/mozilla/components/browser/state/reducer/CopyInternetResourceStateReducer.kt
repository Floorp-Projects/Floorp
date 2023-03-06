/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.reducer

import mozilla.components.browser.state.action.CopyInternetResourceAction
import mozilla.components.browser.state.state.BrowserState

internal object CopyInternetResourceStateReducer {
    fun reduce(state: BrowserState, action: CopyInternetResourceAction): BrowserState {
        return when (action) {
            is CopyInternetResourceAction.AddCopyAction -> updateTheContentState(
                state,
                action.tabId,
            ) {
                it.copy(copy = action.internetResource)
            }

            is CopyInternetResourceAction.ConsumeCopyAction -> updateTheContentState(
                state,
                action.tabId,
            ) {
                it.copy(copy = null)
            }
        }
    }
}
