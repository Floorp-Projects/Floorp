/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.reducer

import mozilla.components.browser.state.action.WebExtensionAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.SessionState
import mozilla.components.browser.state.state.WebExtensionState

internal object WebExtensionReducer {
    /**
     * [WebExtensionAction] Reducer function for modifying a specific [WebExtensionState] in
     *  both [SessionState.extensionState] or [BrowserState.extensions].
     */
    fun reduce(state: BrowserState, action: WebExtensionAction): BrowserState {
        return when (action) {
            is WebExtensionAction.InstallWebExtensionAction -> {
                val existingExtension = state.extensions[action.extension.id]
                if (existingExtension == null) {
                    state.copy(
                        extensions = state.extensions + (action.extension.id to action.extension)
                    )
                } else {
                    state
                }
            }
            is WebExtensionAction.UninstallWebExtensionAction -> {
                state.copy(
                    extensions = state.extensions - action.extensionId,
                    tabs = state.tabs.map { it.copy(extensionState = it.extensionState - action.extensionId) }
                )
            }
            is WebExtensionAction.UpdateWebExtensionEnabledAction -> {
                state.updateWebExtensionState(action.extensionId) {
                    it.copy(enabled = action.enabled)
                }
            }
            is WebExtensionAction.UpdateBrowserAction -> {
                state.updateWebExtensionState(action.extensionId) {
                    it.copy(browserAction = action.browserAction)
                }
            }
            is WebExtensionAction.UpdateBrowserActionPopupSession -> {
                state.updateWebExtensionState(action.extensionId) {
                    it.copy(browserActionPopupSession = action.popupSessionId)
                }
            }
            is WebExtensionAction.UpdateTabBrowserAction -> {
                state.updateWebExtensionTabState(action.sessionId, action.extensionId) {
                    it.copy(browserAction = action.browserAction)
                }
            }
            is WebExtensionAction.UpdateWebExtension -> {
                state.updateWebExtensionState(action.updatedExtension.id) { action.updatedExtension }
            }
        }
    }

    private fun BrowserState.updateWebExtensionTabState(
        tabId: String,
        extensionId: String,
        update: (WebExtensionState) -> WebExtensionState
    ): BrowserState {
        return copy(
            tabs = tabs.map { current ->
                if (current.id == tabId) {
                    val existingExtension = current.extensionState[extensionId]
                    val newExtension = extensionId to update(existingExtension ?: WebExtensionState(extensionId))
                    val updatedExtensions = if (existingExtension == null) {
                        current.extensionState + newExtension
                    } else {
                        val newExtensions = current.extensionState - extensionId
                        newExtensions + newExtension
                    }
                    current.copy(extensionState = updatedExtensions)
                } else {
                    current
                }
            }
        )
    }

    private fun BrowserState.updateWebExtensionState(
        extensionId: String,
        update: (WebExtensionState) -> WebExtensionState
    ): BrowserState {
        val existingExtension = extensions[extensionId]
        val newExtension = extensionId to update(existingExtension ?: WebExtensionState(extensionId))
        val updatedExtensions = extensions - extensionId
        return copy(extensions = updatedExtensions + newExtension)
    }
}
