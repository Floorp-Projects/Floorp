/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.reducer

import mozilla.components.browser.state.action.WebExtensionAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.SessionState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.state.WebExtensionState

internal object WebExtensionReducer {
    /**
     * [WebExtensionAction] Reducer function for modifying a specific [WebExtensionState] in
     *  both [SessionState.extensionState] or [BrowserState.extensions].
     */
    fun reduce(state: BrowserState, action: WebExtensionAction): BrowserState {
        return when (action) {
            is WebExtensionAction.InstallWebExtension -> {
                val existingExtension = state.extensions[action.extension.id]
                if (existingExtension == null) {
                    state.copy(
                        extensions = state.extensions + (action.extension.id to action.extension)
                    )
                } else {
                    state
                }
            }
            is WebExtensionAction.UpdateBrowserAction -> {
                val newExtension = action.extensionId to WebExtensionState(
                    id = action.extensionId,
                    browserAction = action.browserAction
                )

                val updatedExtensions = state.extensions - action.extensionId

                state.copy(
                    extensions = updatedExtensions + newExtension
                )
            }
            is WebExtensionAction.UpdateBrowserActionPopupSession -> {
                val existingExtension = state.extensions[action.extensionId]

                if (existingExtension == null) {
                    state
                } else {
                    val newExtension = action.extensionId to existingExtension.copy(
                        browserActionPopupSession = action.popupSessionId
                    )

                    val updatedExtensions = state.extensions - action.extensionId

                    state.copy(
                        extensions = updatedExtensions + newExtension
                    )
                }
            }

            is WebExtensionAction.UpdateTabBrowserAction -> {
                state.updateTabState(action.sessionId) { tab ->
                    val existingExtension = tab.extensionState[action.extensionId]

                    val newExtension = action.extensionId to WebExtensionState(
                        id = action.extensionId, browserAction = action.browserAction
                    )

                    val updatedExtensions = if (existingExtension == null) {
                        tab.extensionState + newExtension
                    } else {
                        val newExtensions = tab.extensionState - action.extensionId
                        newExtensions + newExtension
                    }

                    tab.copy(
                        extensionState = updatedExtensions
                    )
                }
            }
        }
    }

    private fun BrowserState.updateTabState(
        tabId: String,
        update: (TabSessionState) -> TabSessionState
    ): BrowserState {
        return copy(
            tabs = tabs.map { current ->
                if (current.id == tabId) {
                    update(current)
                } else {
                    current
                }
            }
        )
    }
}
