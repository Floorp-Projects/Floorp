/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.compose.browser.browser

import mozilla.components.lib.state.Store

/**
 * [Store] for maintaining the state of the browser screen.
 */
class BrowserScreenStore(
    initialState: BrowserScreenState = BrowserScreenState(),
) : Store<BrowserScreenState, BrowserScreenAction>(
    initialState = initialState,
    reducer = ::reduce,
)

private fun reduce(state: BrowserScreenState, action: BrowserScreenAction): BrowserScreenState {
    return when (action) {
        is BrowserScreenAction.ToggleEditMode -> state.copy(
            editMode = action.editMode,
            editText = if (action.editMode) null else state.editText,
        )
        is BrowserScreenAction.UpdateEditText -> state.copy(editText = action.text)
        is BrowserScreenAction.ShowTabs -> state.copy(showTabs = true)
        is BrowserScreenAction.HideTabs -> state.copy(showTabs = false)
    }
}
