/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.reducer

import mozilla.components.browser.state.action.UndoAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.UndoHistoryState

internal object UndoReducer {
    /**
     * [UndoAction] Reducer function for modifying the [UndoHistoryState] used to undo the removal
     * of tabs.
     */
    fun reduce(state: BrowserState, action: UndoAction): BrowserState {
        return when (action) {
            is UndoAction.AddRecoverableTabs -> {
                // A middleware will take care of observing tabs getting removed and will then
                // dispatch an this action to remember those tabs. We only remember the last set
                // of tabs that got removed and replace them here.
                state.copy(
                    undoHistory = UndoHistoryState(action.tag, action.tabs, action.selectedTabId),
                )
            }

            is UndoAction.RestoreRecoverableTabs -> {
                // The actual restore is handled by a middleware. Here we only need to clear the
                // state since we assume it got restored.
                state.copy(
                    undoHistory = UndoHistoryState(),
                )
            }

            is UndoAction.ClearRecoverableTabs -> {
                if (action.tag == state.undoHistory.tag) {
                    state.copy(
                        undoHistory = UndoHistoryState(),
                    )
                } else {
                    state
                }
            }
        }
    }
}
