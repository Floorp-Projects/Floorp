/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.reducer

import androidx.annotation.VisibleForTesting
import mozilla.components.browser.state.action.ShareInternetResourceAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.ContentState

internal object ShareInternetResourceStateReducer {
    fun reduce(state: BrowserState, action: ShareInternetResourceAction): BrowserState {
        return when (action) {
            is ShareInternetResourceAction.AddShareAction -> updateTheContentState(state, action.tabId) {
                it.copy(share = action.internetResource)
            }
            is ShareInternetResourceAction.ConsumeShareAction -> updateTheContentState(state, action.tabId) {
                it.copy(share = null)
            }
        }
    }
}

@VisibleForTesting
internal inline fun updateTheContentState(
    state: BrowserState,
    tabId: String,
    crossinline update: (ContentState) -> ContentState,
): BrowserState {
    return state.updateTabOrCustomTabState(tabId) { current ->
        current.createCopy(content = update(current.content))
    }
}
