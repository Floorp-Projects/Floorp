/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.reducer

import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.ContentState
import mozilla.components.browser.state.state.SessionState

internal object ContentStateReducer {
    /**
     * [ContentAction] Reducer function for modifying a specific [ContentState] of a [SessionState].
     */
    @Suppress("LongMethod")
    fun reduce(state: BrowserState, action: ContentAction): BrowserState {
        return when (action) {
            is ContentAction.RemoveIconAction -> updateContentState(state, action.sessionId) {
                it.copy(icon = null)
            }
            is ContentAction.RemoveThumbnailAction -> updateContentState(state, action.sessionId) {
                it.copy(thumbnail = null)
            }
            is ContentAction.UpdateUrlAction -> updateContentState(state, action.sessionId) {
                it.copy(url = action.url)
            }
            is ContentAction.UpdateProgressAction -> updateContentState(state, action.sessionId) {
                it.copy(progress = action.progress)
            }
            is ContentAction.UpdateTitleAction -> updateContentState(state, action.sessionId) {
                it.copy(title = action.title)
            }
            is ContentAction.UpdateLoadingStateAction -> updateContentState(state, action.sessionId) {
                it.copy(loading = action.loading)
            }
            is ContentAction.UpdateSearchTermsAction -> updateContentState(state, action.sessionId) {
                it.copy(searchTerms = action.searchTerms)
            }
            is ContentAction.UpdateSecurityInfoAction -> updateContentState(state, action.sessionId) {
                it.copy(securityInfo = action.securityInfo)
            }
            is ContentAction.UpdateIconAction -> updateContentState(state, action.sessionId) {
                it.copy(icon = action.icon)
            }
            is ContentAction.UpdateThumbnailAction -> updateContentState(state, action.sessionId) {
                it.copy(thumbnail = action.thumbnail)
            }
            is ContentAction.UpdateDownloadAction -> updateContentState(state, action.sessionId) {
                it.copy(download = action.download)
            }
            is ContentAction.ConsumeDownloadAction -> updateContentState(state, action.sessionId) {
                if (it.download != null && it.download.id == action.downloadId) {
                    it.copy(download = null)
                } else {
                    it
                }
            }
            is ContentAction.UpdateHitResultAction -> updateContentState(state, action.sessionId) {
                it.copy(hitResult = action.hitResult)
            }
            is ContentAction.ConsumeHitResultAction -> updateContentState(state, action.sessionId) {
                it.copy(hitResult = null)
            }
            is ContentAction.UpdatePromptRequestAction -> updateContentState(state, action.sessionId) {
                it.copy(promptRequest = action.promptRequest)
            }
            is ContentAction.ConsumePromptRequestAction -> updateContentState(state, action.sessionId) {
                it.copy(promptRequest = null)
            }
            is ContentAction.AddFindResultAction -> updateContentState(state, action.sessionId) {
                it.copy(findResults = it.findResults + action.findResult)
            }
            is ContentAction.ClearFindResultsAction -> updateContentState(state, action.sessionId) {
                it.copy(findResults = emptyList())
            }
        }
    }
}

private fun updateContentState(
    state: BrowserState,
    tabId: String,
    update: (ContentState) -> ContentState
): BrowserState {
    // Currently we map over both lists (tabs and customTabs). We could optimize this away later on if we know what
    // type we want to modify.
    return state.copy(
        tabs = state.tabs.map { current ->
            if (current.id == tabId) {
                current.copy(content = update.invoke(current.content))
            } else {
                current
            }
        },
        customTabs = state.customTabs.map { current ->
            if (current.id == tabId) {
                current.copy(content = update.invoke(current.content))
            } else {
                current
            }
        }
    )
}
