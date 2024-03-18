/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.reducer

import mozilla.components.browser.state.action.ExtensionsProcessAction
import mozilla.components.browser.state.state.BrowserState

internal object ExtensionsProcessStateReducer {

    fun reduce(state: BrowserState, action: ExtensionsProcessAction): BrowserState = when (action) {
        is ExtensionsProcessAction.ShowPromptAction -> state.copy(showExtensionsProcessDisabledPrompt = action.show)
        is ExtensionsProcessAction.DisabledAction -> state.copy(extensionsProcessDisabled = true)
        is ExtensionsProcessAction.EnabledAction -> state.copy(extensionsProcessDisabled = false)
    }
}
