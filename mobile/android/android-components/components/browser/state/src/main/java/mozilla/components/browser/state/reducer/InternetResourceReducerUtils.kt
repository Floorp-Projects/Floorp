/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.reducer

import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.ContentState

internal inline fun updateTheContentState(
    state: BrowserState,
    tabId: String,
    crossinline update: (ContentState) -> ContentState,
): BrowserState {
    return state.updateTabOrCustomTabState(tabId) { current ->
        current.createCopy(content = update(current.content))
    }
}
