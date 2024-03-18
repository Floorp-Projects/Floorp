/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.reducer

import mozilla.components.browser.state.action.LocaleAction
import mozilla.components.browser.state.state.BrowserState

internal object LocaleStateReducer {

    /**
     * [LocaleAction] Reducer function for modifying [BrowserState.locale].
     */
    fun reduce(state: BrowserState, action: LocaleAction): BrowserState {
        return when (action) {
            is LocaleAction.UpdateLocaleAction -> state.copy(locale = action.locale)
            is LocaleAction.RestoreLocaleStateAction -> state
        }
    }
}
